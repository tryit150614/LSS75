#include <stdio.h>
#include <string.h>
#include <cmath>
#include <stdlib.h>

#include "fsm.h"
#include "main.h"
#include "serialHandlers.h"
#include "ringBuffer1.h"
#include "usart.h"
#include "adc.h"
#include "nodeManager.h"
#include "eeprom.h"
#include "st_rtc.h"
#include "st_watchdog.h"
#include "rs485_list.h"
#include "EXS62.h"
#include "gpio.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "FreeRTOSConfig.h"

#include "stm32l4xx_hal_pwr.h"

extern struct inputString inputStringUsart2;
extern volatile ringBufferStruct usart2Buf;

extern struct inputString inputStringUsart3;
extern volatile ringBufferStruct usart3Buf;
extern const unsigned int usart3DataSize = 200;

extern struct inputString inputStringLpuart1;
extern volatile ringBufferStruct lpuart1Buf;
extern int unsigned lpuart1DataLength;

extern volatile uint32_t pResendTime;
extern volatile uint32_t pMAC;
extern char pHash[9];

extern volatile bool LED_RS485_Flag;
extern volatile bool LED_Module_Flag;
extern volatile bool LED_SendOK_Flag;
extern bool adc_Flag;

extern EEPROMFlag eepromFlag;
extern Watchdog watchdog;
extern Rtc rtc;

extern char batV[10], solarV[10], innerT[10];

/* Parameter from RS485 */
static char     soilTemp[20]      = "0.0";
static char     soilVWC [20]      = "0";
static char     soilEC  [20]      = "0.0";
static char     soilPH  [20]      = "0.0";
static char     airT [20]         = "0.0";
static char     airH[20]          = "0.0";
static char     solra [10]        = "0.0";
static char     waterDepth [10]   = "0.0";
static char     sunRadiation[10]  = "0";
static char     windSpeed [10]    = "0.0";
static char     leafH  [10]       = "0.0";
static char     leafT  [10]       = "0.0";
static char     dewPoint[10]      = "0.0";

static char     newData[256+1]         = {0};
static char     readySendData[10500+1] = {0};
static uint32_t amountOfData     = 0;

char RSSI[5] = {0};

struct FSMAttributes fsmAttributes;

static uint8_t nonData[payloadLen + 1]   = {0};
static bool noConnectFlag = false;
// Declare private functions---------
static void transmitRS485(uint8_t * deviceNumber);
static void getRS485Data(char * deviceData, int receiveDataLen);
static void eeprom_ClearData();
//-----------------------------------

void TaskFSM(void *pvParameters) {
  for (;;) {
      if(!adc_Flag) vTaskDelay(pdMS_TO_TICKS(100));
      fsmAttributes.state = (StateFunc)(*fsmAttributes.state)();
  }
}

void fsm_init()
{
  fsmAttributes.state = fsm_idle;
  memset(RSSI, 0, sizeof(RSSI));
  memset(newData, 0, sizeof(newData));
  memset(readySendData, 0, sizeof(readySendData));

  for(int i = 0; i < payloadLen; i++)
    nonData[i] = 0xFF;
}

funcptr fsm_idle()
{
  powerOnRS485IC();
  M1_PowerOn();

  return (funcptr)fsm_getRS485Data;
}

funcptr fsm_getRS485Data()
{
  DEBUG(">>fsm_getRS485Data<<\r\n");

  char rs485Data_Soil[11 * 2 + 1]      = {0};
  char rs485Data_Soil_2[11 * 2 + 1]    = {0};
  char rs485Data_PH[9 * 2 + 1]         = {0};
  char rs485Data_PH_2[7 * 2 + 1]       = {0};
  char rs485Data_Air[11 * 2 + 1]       = {0};
  char rawData_Solar[7 * 2 + 1]        = {0};
  char rawData_Waterdepth[7 * 2 + 1]   = {0};
  char rawData_Waterdepth_2[7 * 2 + 1] = {0};
  char rawData_SR[7 * 2 + 1]           = {0};
  char rawData_WS[7 * 2 + 1]           = {0};
  char rawData_WS_2[7 * 2 + 1]         = {0};
  char rawData_Leaf[9 * 2 + 1]         = {0};

  powerOnRS485Sensor();
  vTaskDelay(pdMS_TO_TICKS(10000));
  memset(usart3Buf.bufferPtr, 0, usart3DataSize);
  ringBuffer_resetBuffer(&usart3Buf);

  transmitRS485(Infwin_PH_Sensor);
  getRS485Data(rs485Data_PH, Infwin_soil_3on1_ACK_BYTES);
  transmitRS485(Laytion_soil_3on1_Sensor);
  getRS485Data(rs485Data_Soil_2, Infwin_PH_ACK_BYTES);
  transmitRS485(Infwin_air_Sensor);
  getRS485Data(rs485Data_Air, Laytion_soil_3on1_ACK_BYTES);
  transmitRS485(Laytion_PH_Sensor);
  getRS485Data(rs485Data_PH_2, Infwin_air_ACK_BYTES);
  transmitRS485(Infwin_soil_3on1_Sensor);
  getRS485Data(rs485Data_Soil, Laytion_PH_ACK_BYTES);
  transmitRS485(Laytion_Solar_Sensor);
  getRS485Data(rawData_Solar, Laytion_Solar_ACK_BYTES);
  transmitRS485(Laytion_waterDepth_Sensor);
  getRS485Data(rawData_Waterdepth, Laytion_waterDepth_ACK_BYTES);
  transmitRS485(Infwin_sunRadiation_Sensor);
  getRS485Data(rawData_SR, Infwin_sunRadiation_ACK_BYTES);
  transmitRS485(PRS_windSpeed_Sensor);
  getRS485Data(rawData_WS, PRS_windSpeed_ACK_BYTES);
  transmitRS485(Laytion_windSpeed_Sensor);
  getRS485Data(rawData_WS_2, Laytion_windSpeed_ACK_BYTES);
  transmitRS485(Laytion_leaf_Sensor);
  getRS485Data(rawData_Leaf, Laytion_leaf_ACK_BYTES);
  transmitRS485(Laytion_waterDepth_New_Sensor);
  getRS485Data(rawData_Waterdepth_2, Laytion_waterDepth_ACK_BYTES);

  powerOffRS485Sensor();
  
  DEBUG("\r\n--------- Infwin Sensor ---------\r\n");

  if(strnlen(rs485Data_Soil, sizeof(rs485Data_Soil)) == 0){
    DEBUG("Soil senser : N/A\r\n");
  }
  else {
    LED_RS485_Flag = true;
    DEBUG("Soil sensor data : ");DEBUG(rs485Data_Soil);DEBUG("\r\n");

    char tempStr[20]  = {0};
    uint32_t tempUint = 0;
    /* Soil temp */
    memcpy(tempStr, rs485Data_Soil + 6, 4);
    tempUint = (uint32_t)strtol(tempStr, NULL, 16); // need/100 ex:1794 -> 17.94
    if(tempUint <= 20000) { // temp <= 200.00 C
      sprintf(soilTemp, "%lu.%02lu", tempUint / 100, tempUint % 100);
    }
    else {
      sprintf(soilTemp, "-%lu.%02lu", (65536 - tempUint + 1) / 100, (65536 - tempUint + 1) % 100);
    }

    /* Soil VMC */
    memset(tempStr, 0, 20);
    memcpy(tempStr, rs485Data_Soil + 10, 4);
    tempUint = (uint32_t)strtol(tempStr, NULL, 16) / 100;
    sprintf(soilVWC, "%lu", tempUint);
    DEBUG("SoilVMC : ");DEBUG(soilVWC);DEBUG("\r\n");

    /* Soil EC */
    memset(tempStr, 0, 20);
    memcpy(tempStr, rs485Data_Soil + 14, 4);
    tempUint = (uint32_t)strtol(tempStr, NULL, 16);
    sprintf(soilEC, "%lu.%03lu", tempUint / 100, (tempUint % 100) * 10);
  }

  if(strnlen(rs485Data_PH, sizeof(rs485Data_PH)) == 0){
    DEBUG("PH senser : N/A\r\n");
  }
  else {
    LED_RS485_Flag = true;
    DEBUG("PH sensor data : ");DEBUG(rs485Data_PH);DEBUG("\r\n");

    /* PH */
    char tempSoilPH[20] = {0};
    uint32_t tempPH = 0;

    memcpy(tempSoilPH, rs485Data_PH + 10, 4);
    tempPH = (uint32_t)strtol(tempSoilPH, NULL, 16); // 700 -> 7.00
    sprintf(soilPH, "%lu.%02lu", tempPH / 100, tempPH % 100);
  }
  
  if(strnlen(rs485Data_Air, sizeof(rs485Data_Air)) == 0){
    DEBUG("Air senser : N/A\r\n");
  }
  else {
    LED_RS485_Flag = true;
    DEBUG("Air sensor data : ");DEBUG(rs485Data_Air);DEBUG("\r\n");

    /* Air temp */
    char tempAirT[5] = {0};
    uint16_t tempAT = 0;

    memcpy(tempAirT, rs485Data_Air + 6, 4);
    tempAT = (uint16_t)strtol(tempAirT, NULL, 16); // 3000 (30.00)
    if(tempAT <= 20000) { // temp <= 200.00 C
      sprintf(airT, "%u.%02u", tempAT / 100, tempAT % 100);
    }
    else {
      sprintf(airT, "%u.%02u", (65536 - tempAT) / 100, (65536 - tempAT) % 100);
    }

    /* Air H */
    char tempAirHStr[5] = {0};
    uint16_t tempAH = 0;

    memcpy(tempAirHStr, rs485Data_Air + 10, 4);
    tempAH = (uint16_t)strtol(tempAirHStr, NULL, 16); // 3000 (30.00%)
    sprintf(airH, "%u.%02u", tempAH / 100, tempAH % 100);

    /* DewPoint */
    float _hu = tempAH/10 + (float)(tempAH%10)/10.0;
    float _te = tempAT/10 + (float)(tempAT%10)/10.0;
    float gm = (17.271 * (float)_te / (237.7 + (float)_te)) + log(_hu / 100.0);
    float _dewpoint = (237.7 * gm) / (17.271 - gm);
    sprintf(dewPoint, "%d.%02d", (int)_dewpoint, (int)(_dewpoint*100)%100);
  }

  DEBUG("\r\n--------- PRS Sensor ---------\r\n");
  /* Wind Speed */
  if(strnlen(rawData_WS, sizeof(rawData_WS)) == 0){
    DEBUG("Wind speed senser : N/A\r\n");
  }
  else {
    LED_RS485_Flag = true;
    DEBUG("Wind speed sensor data : ");DEBUG(rawData_WS);DEBUG("\r\n");

    char tempWSStr[20] = {0};
    uint32_t tempWS = 0;

    memcpy(tempWSStr, rawData_WS + 6, 4);
    tempWS = (uint32_t)strtol(tempWSStr, NULL, 16);
    sprintf(windSpeed, "%lu.%01lu", tempWS / 10, tempWS % 10);
  }

  DEBUG("\r\n--------- Laytion Sensor ---------\r\n");

  if(strnlen(rs485Data_Soil_2, sizeof(rs485Data_Soil_2)) == 0){
    DEBUG("Soil_2 senser : N/A\r\n");
  }
  else {
    LED_RS485_Flag = true;
    DEBUG("Soil_2 sensor data : ");DEBUG(rs485Data_Soil_2);DEBUG("\r\n");

    /* Soil temp */
    char tempSoilTemp[20] = {0};
    uint32_t tempST = 0;

    memcpy(tempSoilTemp, rs485Data_Soil_2 + 6, 4);
    tempST = (uint32_t)strtol(tempSoilTemp, NULL, 16);
    if(tempST < 8000) {
      // 235 -> 23.50
      sprintf(soilTemp, "%lu.%02lu", tempST / 10, tempST % 10 * 10);
    }
    else {
      tempST = (0xffff - tempST + 1); // 35
      sprintf(soilTemp, "-%lu.%02lu", tempST / 10, tempST % 10 * 10); // 35 -> -3.50
    }

    /* Soil VMC */
    char tempVMCStr[20] = {0};
    uint32_t tempVMC = 0;

    memcpy(tempVMCStr, rs485Data_Soil_2 + 10, 4);
    tempVMC = (uint32_t)strtol(tempVMCStr, NULL, 16) / 10;
    sprintf(soilVWC, "%lu", tempVMC);

    /* Soil EC */
    char tempECStr[20] = {0};
    uint32_t tempEC = 0;

    memcpy(tempECStr, rs485Data_Soil_2 + 14, 4);
    tempEC = (uint32_t)strtol(tempECStr, NULL, 16);
    sprintf(soilEC, "%lu.%03lu", tempEC / 100, (tempEC % 100) * 10);
  }

  if(strnlen(rs485Data_PH_2, sizeof(rs485Data_PH_2)) == 0){
    DEBUG("PH_2 senser : N/A\r\n");
  }
  else {
    LED_RS485_Flag = true;
    DEBUG("PH_2 sensor data : ");DEBUG(rs485Data_PH_2);DEBUG("\r\n");

    /* PH */
    char tempSoilPH[5] = {0};
    uint16_t tempPH = 0;

    memcpy(tempSoilPH, rs485Data_PH_2 + 6, 4);
    tempPH = (uint16_t)strtol(tempSoilPH, NULL, 16);
    sprintf(soilPH, "%u.%02u", tempPH / 100, tempPH % 100);
  }

  /* SolraPAR */
  if(strnlen(rawData_Solar, sizeof(rawData_Solar)) == 0){
    DEBUG("Solra PAR senser : N/A\r\n");
  }
  else {
    LED_RS485_Flag = true;
    DEBUG("Solra PAR sensor data : ");DEBUG(rawData_Solar);DEBUG("\r\n");

    char tempStr[20] = {0};
    uint32_t tempSR = 0;

    memcpy(tempStr, rawData_Solar + 6, 4);
    tempSR = (uint32_t)strtol(tempStr, NULL, 16);
    sprintf(solra, "%lu.%01lu", tempSR / 10, tempSR % 10);
  }

  /* Water depth */ 
  if(strnlen(rawData_Waterdepth, sizeof(rawData_Waterdepth)) == 0){
    DEBUG("Water depth senser : N/A\r\n");
  }
  else {
    LED_RS485_Flag = true;
    DEBUG("water depth sensor data : ");DEBUG(rawData_Waterdepth);DEBUG("\r\n");

    char tempStr[20] = {0};
    uint32_t tempWD = 0;

    memcpy(tempStr, rawData_Waterdepth + 6, 4);
    tempWD = (uint32_t)strtol(tempStr, NULL, 16);
    sprintf(waterDepth, "%lu", tempWD);
  }

  /* Sun Radiation */
  if(strnlen(rawData_SR, sizeof(rawData_SR)) == 0){
    DEBUG("Sun Radiation senser : N/A\r\n");
  }
  else {
    LED_RS485_Flag = true;
    DEBUG("Sun Radiation sensor data : ");DEBUG(rawData_SR);DEBUG("\r\n");

    char tempStr[20] = {0};
    uint32_t tempSR = 0;

    memcpy(tempStr, rawData_SR + 6, 4);
    tempSR = (uint32_t)strtol(tempStr, NULL, 16);
    sprintf(sunRadiation, "%lu", tempSR);
  }

  /* Wind Speed 2 */
  if(strnlen(rawData_WS_2, sizeof(rawData_WS_2)) == 0){
    DEBUG("Wind speed senser : N/A\r\n");
  }
  else {
    LED_RS485_Flag = true;
    DEBUG("Wind speed sensor 2 data : ");DEBUG(rawData_WS_2);DEBUG("\r\n");

    char tempStr[20] = {0};
    uint32_t tempWS = 0;

    memcpy(tempStr, rawData_WS_2 + 6, 4);
    tempWS = (uint32_t)strtol(tempStr, NULL, 16);
    sprintf(windSpeed, "%lu.%01lu", tempWS / 100, (tempWS % 100) / 10);
  }
  
  /* Leaf T&H */
  if(strnlen(rawData_Leaf, sizeof(rawData_Leaf)) == 0){
    DEBUG("Leaf T&H senser : N/A\r\n");
  }
  else {
    LED_RS485_Flag = true;
    DEBUG("Leaf T&H sensor data : ");DEBUG(rawData_Leaf);DEBUG("\r\n");

    /* Leaf temp */
    char tempLeafT[5] = {0};
    uint16_t tempLT = 0;

    memcpy(tempLeafT, rawData_Leaf + 6, 4);
    tempLT = (uint16_t)strtol(tempLeafT, NULL, 16);
    if(tempLT <= 8000) { 
      sprintf(leafT, "%u.%02u", tempLT / 100, tempLT % 100);
    }
    else {
      sprintf(leafT, "-%u.%02u", (65536 - tempLT + 1) / 10, (65536 - tempLT + 1) % 10 * 10);
    }

    /* Leaf H */
    char tempLeafH[5] = {0};
    uint16_t tempLH = 0;

    memcpy(tempLeafH, rawData_Leaf + 10, 4);
    tempLH = (uint16_t)strtol(tempLeafH, NULL, 16); // 356 (35.6)
    sprintf(leafH, "%d.%01d", tempLH / 10, tempLH % 10);
  }

  /* New Water depth */ 
  if(strnlen(rawData_Waterdepth_2, sizeof(rawData_Waterdepth_2)) == 0){
    DEBUG("New water depth sensor : N/A\r\n");
  }
  else {
    LED_RS485_Flag = true;
    DEBUG("New water depth sensor : ");DEBUG(rawData_Waterdepth_2);DEBUG("\r\n");

    char tempStr[20] = {0};
    uint32_t tempWD = 0;

    memcpy(tempStr, rawData_Waterdepth_2 + 6, 4);
    tempWD = (uint32_t)strtol(tempStr, NULL, 16);
    sprintf(waterDepth, "%lu.0", tempWD);
  }

  DEBUG("----------------------------------\r\n");
  return (funcptr)fsm_initModule;
}

funcptr fsm_initModule()
{
  static bool reInitFlag = false;

  M1_PowerOn();
  /* 等待開機訊息 */
  if(exs62_ready()) DEBUG("Fail to start EXS62\r\n");
  /* 初始化HTTP參數 */
  if(exs62_setHttp()) {
    DEBUG("EXS62 setting Fail\r\n");
    LED_Module_Flag = false;
    if(!reInitFlag) {
      reInitFlag = true;
      M1_PowerOff();
      vTaskDelay(pdMS_TO_TICKS(1000));
      return (funcptr)fsm_initModule;
    }
  }

  LED_Module_Flag = true;
  /*  等待撥接到網路 */
  if(!exs62_connectNetwork()) noConnectFlag = false;
  else {                      noConnectFlag = true;
    RSSI[0] = '0';
    DEBUG("Connect network fail\r\n");
    return (funcptr)fsm_prepareNewPayload;
  }

  return (funcptr)fsm_getRSSI;
}

funcptr fsm_getRSSI() {
  static int getRSSI_count = 0;
  memset(RSSI, 0, sizeof(RSSI));
  if(exs62_getRSSI(RSSI)) {
    getRSSI_count++;
    if(getRSSI_count >= 3) {
      RSSI[0] = '0';
    }
    else return (funcptr)fsm_getRSSI;
  }

  getRSSI_count = 0;
  DEBUG("RSSI = ");DEBUG(RSSI);DEBUG("\r\n");
  return (funcptr)fsm_prepareNewPayload;
}

funcptr fsm_prepareNewPayload() {
  sprintf(newData, "{\"device_id\":%lu,\"hash\":\"%s\",\"data\":{\"st\":%s,\"sm\":%s,\"sec\":%s,\"sph\":%s,\"at\":%s,\"ah\":%s,\"dp\":%s,\"spar\":%s,\"wdep\":%s,\"sr\":%s,\"ws\":%s,\"lt\":%s,\"lh\":%s,\"it\":%s,\"sv\":%s,\"volt\":%s,\"rssi\":%s,\"snr\":%s},\"gts\":%lu}",
            pMAC,
            pHash,
            soilTemp,
            soilVWC,
            soilEC,
            soilPH,
            airT,
            airH,
            dewPoint,      // DewPoint
            solra,
            waterDepth,
            sunRadiation,
            windSpeed,
            leafT,
            leafH,
            innerT,
            solarV,
            batV,
            RSSI,
            "0",         // SNR
            getUnixTimestamp());
  
  strcat(readySendData, newData);
  amountOfData = 1;
  DEBUG("******** Payload ********\r\n");
  DEBUG(newData);
  DEBUG("\r\n******** ******* ********\r\n");

  if(noConnectFlag) {
    DEBUG("Save in eeprom.\r\n");
    eeprom_saveData_and_jumpNextPos((uint8_t*)newData);
    return (funcptr)fsm_sleep;
  }

  return (funcptr)fsm_checkEEPROM;
}

static uint32_t dataPos = 1;
funcptr fsm_checkEEPROM() {
  uint8_t eepromData[payloadLen + 1] = {0};
  uint32_t addr = eepromFlag.nextSendAddress + (dataPos - 1) * payloadLen;
  if(addr >= MAX_EEPROM_SIZE) addr = dataFirstAddress;

  bool sts = parLoad(eepromData, addr, payloadLen);
  DEBUG(">>NextSendPos : ");writeUint32(DEBUG_USART, addr);DEBUG("\r\n");

  if(!sts || eepromData[0] == 0xFF || amountOfData >= 20) {
    DEBUG("End, Amount of data : ");writeUint32(DEBUG_USART, amountOfData);DEBUG("\r\n");
    return (funcptr)fsm_sendData;
  }
  else if(strnlen((char *)eepromData, payloadLen) <= 150) {
    DEBUG("This data have problem, jump it.\r\n");
    dataPos++;
    return (funcptr)fsm_checkEEPROM;
  }
  else {
    DEBUG("Get data in eeprom.\r\n");
    strcat(readySendData, ",");
    strcat(readySendData, (char *)eepromData);
    amountOfData++;
    dataPos++;
    return (funcptr)fsm_checkEEPROM;
  }
}

funcptr fsm_sendData() {

  char tempArray[12000] = {0};
  int len = sprintf(tempArray, "{\"num\":%lu,\"records\":[%s]}",
            amountOfData,
            readySendData
            );

  if(exs62_preparePayload_Connect(len)) DEBUG("exs62_preparePayload_Connect fail\r\n");
  vTaskDelay(pdMS_TO_TICKS(100));
  if(exs62_http_send(tempArray, len))   DEBUG("exs62_http_send fail\r\n");
  return (funcptr)fsm_getResponse;
}

funcptr fsm_getResponse() {
  /* If http state is 200 or other */
  int result = exs62_getResponse(5, "^SISR: 6,1");
  if(result) { /* Not 200 */
    DEBUG("Result : Fail!\r\n");

    /* 關閉連線 */
    if(exs62_closeConnetion())  DEBUG("Close connection fail!\r\n");
    else                        DEBUG("Close connection OK\r\n");

    /* 連續422則清空EEPROM */
    uint8_t checkEEPROM[2] = {0}; // 位置8為清空eepromFlag, 位置9為紀錄422次數
    parLoad(checkEEPROM, 9, 1);
    if     (result != 422 && checkEEPROM[1] >= 1) {
      /*清空422計算, 連續才繼續加*/
      checkEEPROM[0] = 0;
      parSave(checkEEPROM, 9, 1);
    }
    else if(result == 422 && checkEEPROM[0] == 0) {
      checkEEPROM[0] = 1;
      parSave(checkEEPROM, 9, 1);
    }
    else if(result == 422 && checkEEPROM[0] >= 1) {
      DEBUG("Get 422 Unprocessable Entity >= 2, clear eeprom after wakeup\r\n");
      checkEEPROM[0] = 1;
      checkEEPROM[1] = 0;
      parSave(checkEEPROM, 8, 2);
      /* 關閉EXS62 */
      if(!exs62_switchOff()) DEBUG("Successful to power off EXS62\r\n");
      M1_PowerOff();
      return (funcptr)fsm_sleep;
    }

    //儲存最新一筆資料
    DEBUG("Save in eeprom.\r\n");
    eeprom_saveData_and_jumpNextPos((uint8_t*)newData);

    return (funcptr)fsm_sleep;
  }

  /* State 200 */
  DEBUG("Result :OK!\r\n");
  LED_SendOK_Flag = true;
  vTaskDelay(pdMS_TO_TICKS(1000));

  eeprom_ClearData();

  sendCmd("AT^SISR=6,1500\r\n");
  exs62_getResponse(3, "\"ack\"");

  DEBUG("=================\r\n");
  DEBUG(inputStringLpuart1.string);
  DEBUG("\r\n=================\r\n");

  /* Get resendTime and rtc time */
  char *part = strstr(inputStringLpuart1.string, "\"ack\":");
  uint32_t startPos = part - inputStringLpuart1.string;
  char resendTime[10] = {0};
  char timestemp[15] = {0};

  int i = 0;
  while((inputStringLpuart1.string[startPos + 7 + i] != '|') && i < 10)
  {
    resendTime[i] = inputStringLpuart1.string[startPos + 7 + i];
    i++;
  }
  /* Now pos is '|' */
  memcpy(timestemp, inputStringLpuart1.string + startPos + 7 + i + 1, 12);
  DEBUG("New ResendTime :");DEBUG(resendTime);DEBUG(", time :");DEBUG(timestemp);DEBUG("\r\n");

  /* Resend time */
  uint32_t getResendTime = (uint32_t)strtol(resendTime, NULL, 10);
  if(getResendTime != pResendTime && getResendTime > 60) {
    DEBUG("Change RT from ");DEBUG_uint(pResendTime);
    DEBUG(" to ");DEBUG_uint(getResendTime);DEBUG("\r\n");
    nodeManager_changeResendTime((uint64_t)getResendTime);
  }

  /* Gateway time */
  char YY[3], MM[3], DD[3], hh[3], mm[3], ss[3];
  if(timestemp != NULL) {
    memcpy(YY, timestemp, 2);
    memcpy(MM, timestemp+2, 2);
    memcpy(DD, timestemp+4, 2);
    memcpy(hh, timestemp+6, 2);
    memcpy(mm, timestemp+8, 2);
    memcpy(ss, timestemp+10, 2);

    uint32_t result = 0;
    uint32_t year = ((uint32_t)YY[0] - 0x30) * 10 + (uint32_t)YY[1] - 0x30;
    uint32_t month = ((uint32_t)MM[0] - 0x30) * 10 + (uint32_t)MM[1] - 0x30;
    if(month > 12 || month <= 0) result = 1;
    uint32_t day = ((uint32_t)DD[0] - 0x30) * 10 + (uint32_t)DD[1] - 0x30;
    if(day > 31 || day <= 0) result = 2;
    if(month == 2 && day > 29) result = 2;

    uint32_t hour = ((uint32_t)hh[0] - 0x30) * 10 + (uint32_t)hh[1] - 0x30;
    if(hour > 24 || hour < 0) result = 3;
    uint32_t minute = ((uint32_t)mm[0] - 0x30) * 10 + (uint32_t)mm[1] - 0x30;
    if(minute >= 60 || minute < 0) result = 4;
    uint32_t second = ((uint32_t)ss[0] - 0x30) * 10 + (uint32_t)ss[1] - 0x30;
    if(second >= 60 || second < 0) result = 5;

    if(result == 0) {
      rtc.setTime(hour, minute, second);
      rtc.setDate(year, month, day);
      DEBUG("Fix rtc time ok.\r\n");
    }
  }

  /* 關閉連線 */
  if(exs62_closeConnetion())  DEBUG("Close connection fail!\r\n");

  return (funcptr)fsm_sleep;
}

funcptr fsm_sleep() {
  /* 關閉EXS62 */
  if(!exs62_switchOff()) DEBUG("Successful to power off EXS62\r\n");
  M1_PowerOff();

  DEBUG("Sleep time : ");writeUint32(DEBUG_USART, pResendTime);DEBUG("s\r\n");
  HAL_PWREx_EnterSHUTDOWNMode();
  return 0;
}

//-------------------private function------------------------------
static void transmitRS485(uint8_t * deviceNumber){
  RS485_Send();
  writeData(USART3, (char *)deviceNumber, 8);
  HAL_Delay(2);

  RS485_Receive();
  HAL_Delay(250);
}

static void getRS485Data(char * deviceData, int receiveDataLen){

  parseHexInBuffer(&inputStringUsart3, &usart3Buf, receiveDataLen);
  if(inputStringUsart3.ready == 1){
    inputStringUsart3.ready = 0;
    binaryToString(inputStringUsart3.string, receiveDataLen, deviceData);
  }
}

static void eeprom_ClearData() {
  /* 刪除EEPROM裡的資料 */
  if(dataPos != 1) {
    uint32_t finalAddr = 0;
    for(uint32_t i = 1; i < dataPos; i++) {
      uint32_t addr = eepromFlag.nextSendAddress + (i - 1) * payloadLen;
      if(addr >= MAX_EEPROM_SIZE) addr = dataFirstAddress;
      parSave(nonData, addr, payloadLen);
      DEBUG("Delete data at : ");writeUint32(DEBUG_USART, addr);DEBUG("\r\n");
      finalAddr = addr + payloadLen;
    }

    changeSendAddress(finalAddr);
  }
}

//-----------------------------------------------------------------
