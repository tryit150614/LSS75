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
#include "SARA-R410M.h"
#include "EXS62.h"

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
extern int lpuart1DataLength;

extern uint16_t adc1DmaBuffer[DMA1_ADC_BUFFER_SIZE];
extern volatile uint32_t pResendTime;
extern volatile uint32_t pMAC;
extern char pHash[9];

extern volatile bool rs485Flag;
extern volatile bool moduleFlag;
extern volatile bool sendOKFlag;

extern EEPROMFlag eepromFlag;
extern Rtc rtc;

/* Payload */
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
static char     newData[256+1]         = {0};
static char     readySendData[10500+1] = {0};
static uint32_t amountOfData     = 0;
static uint32_t tempTimestamp    = 0;

char RSSI[5] = {0};

struct FSMAttributes fsmAttributes;
Watchdog dog;

static uint8_t nonData[payloadLen + 1]   = {0};
static char batV[10] = {0}, solarV[10]   = {0}, innerT[10] = {0};
static bool noConnectFlag = false;

static bool isCatM = false;
// Declare private functions---------
static void voltToTemperature(float tempVolt, char * tempStr);
static void floatToString(float value, char * str, int dot);
static void transmitRS485(uint8_t * deviceNumber);
static void getRS485Data(char * deviceData, int receiveDataLen);
static void eeprom_ClearData();
//-----------------------------------

void fsm_init()
{
  fsmAttributes.state = fsm_idle;
  memset(fsmAttributes.payloadOut, 0, sizeof(fsmAttributes.payloadOut));

  for(int i = 0; i < payloadLen; i++)
    nonData[i] = 0xFF;
}

funcptr fsm_idle()
{
  /* Enable Module's power */
  NBIOT_PowerOn();

  return (funcptr)fsm_getADCValue;
}

funcptr fsm_getADCValue()
{
  // DEBUG(">>fsm_getADCValue<<\r\n");
  
    uint16_t rawData [4][3] = {0};
    float    tempVolt[3]    = {0};           // BAT, Solar, InnerTemp
    float    fixValue[3]    = {9.633, 9.7, 1};
    
    for(uint8_t i = 0; i < 3; i++)
    {   
        ADC_startConversion();
        while(LL_DMA_IsActiveFlag_TC1(DMA1) == 0);
        LL_DMA_ClearFlag_TC1(DMA1);

        rawData[0][i] = adc1DmaBuffer[0];
        rawData[1][i] = adc1DmaBuffer[1];
        rawData[2][i] = adc1DmaBuffer[2];
        /* voltage reference */
        rawData[3][i] = adc1DmaBuffer[3];
    }

    /* Calculate the voltage reference */
    float base = 0;
    for (uint8_t i = 0; i < 3; i++) {
        base += (float) rawData[3][i];
    }
    base /= 3;
    base = (VREFINT_CAL_VREF / 1000) * ((float) *VREFINT_CAL_ADDR / base);

    /* Calculate t1 ~ t3 */
    float avg;
    for (uint8_t i = 0; i < 3; i++) {
        avg = 0;
        for (uint8_t j = 0; j < 3; j++) {
            avg += (float) rawData[i][j];
        }
        avg /= 3;

        tempVolt[i] = base * (avg / 4095) * fixValue[i];
    }

    memset(batV, 0, sizeof(batV));
    memset(solarV, 0, sizeof(solarV));
    memset(innerT, 0, sizeof(innerT));
    //-----------  Votagle  ---------//
    floatToString(tempVolt[0], batV, 10);
    floatToString(tempVolt[1], solarV, 10);
    //-----------Temperature---------//
    voltToTemperature(tempVolt[2], innerT);

    /* Direct battery voltage, if < 2.8V, just sleep. */
    writeString(DEBUG_USART,"BatteryV : ");DEBUG(batV);writeString(DEBUG_USART,"\r\n");
    if (tempVolt[0] < 2.900000 && tempVolt[0] >= 0.00000)
    {

      writeString(DEBUG_USART,"Keep sleep ...\r\n");
      return (funcptr)fsm_sleep;
    }

    tempTimestamp = getUnixTimestamp();

  return (funcptr)fsm_getRS485Data;
}

funcptr fsm_getRS485Data()
{
  DEBUG(">>fsm_getRS485Data<<\r\n");

  char rs485Data_Soil[11 * 2 + 1]   = {0};
  char rs485Data_Soil_2[11 * 2 + 1] = {0};
  char rs485Data_PH[9 * 2 + 1]      = {0};
  char rs485Data_PH_2[7 * 2 + 1]    = {0};
  char rs485Data_Air[11 * 2 + 1]    = {0};
  char rawData_Solar[7 * 2 + 1]     = {0};
  char rawData_Waterdepth[7 * 2 + 1]= {0};
  char rawData_SR[7 * 2 + 1]        = {0};
  char rawData_WS[7 * 2 + 1]        = {0};
  char rawData_WS_2[7 * 2 + 1]      = {0};
  char rawData_Leaf[9 * 2 + 1]      = {0};

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

  powerOffRS485Sensor();
  
  DEBUG("\r\n--------- Infwin Sensor ---------\r\n");

  if(strnlen(rs485Data_Soil, sizeof(rs485Data_Soil)) == 0){
    DEBUG("Soil senser : N/A\r\n");
  }
  else {
    rs485Flag = true;
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
    rs485Flag = true;
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
    rs485Flag = true;
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
      sprintf(airT, "%lu.%02lu", (65536 - tempAT) / 100, (65536 - tempAT) % 100);
    }

    /* Air H */
    char tempAirHStr[5] = {0};
    uint16_t tempAH = 0;

    memcpy(tempAirHStr, rs485Data_Air + 10, 4);
    tempAH = (uint16_t)strtol(tempAirHStr, NULL, 16); // 3000 (30.00%)
    sprintf(airH, "%lu.%02lu", tempAH / 100, tempAH % 100);
  }

  DEBUG("\r\n--------- PRS Sensor ---------\r\n");
  /* Wind Speed */
  if(strnlen(rawData_WS, sizeof(rawData_WS)) == 0){
    DEBUG("Wind speed senser : N/A\r\n");
  }
  else {
    rs485Flag = true;
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
    rs485Flag = true;
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
    rs485Flag = true;
    DEBUG("PH_2 sensor data : ");DEBUG(rs485Data_PH_2);DEBUG("\r\n");

    /* PH */
    char tempSoilPH[5] = {0};
    uint16_t tempPH = 0;

    memcpy(tempSoilPH, rs485Data_PH_2 + 6, 4);
    tempPH = (uint16_t)strtol(tempSoilPH, NULL, 16);
    sprintf(soilPH, "%lu.%02lu", tempPH / 100, tempPH % 100);
  }

  /* SolraPAR */
  if(strnlen(rawData_Solar, sizeof(rawData_Solar)) == 0){
    DEBUG("Solra PAR senser : N/A\r\n");
  }
  else {
    rs485Flag = true;
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
    rs485Flag = true;
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
    rs485Flag = true;
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
    rs485Flag = true;
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
    rs485Flag = true;
    DEBUG("Leaf T&H sensor data : ");DEBUG(rawData_Leaf);DEBUG("\r\n");

    /* Leaf temp */
    char tempLeafT[5] = {0};
    uint16_t tempLT = 0;

    memcpy(tempLeafT, rawData_Leaf + 6, 4);
    tempLT = (uint16_t)strtol(tempLeafT, NULL, 16);
    if(tempLT <= 8000) { 
      sprintf(leafT, "%lu.%02lu", tempLT / 100, tempLT % 100);
    }
    else {
      sprintf(leafT, "-%lu.%02lu", (65536 - tempLT + 1) / 10, (65536 - tempLT + 1) % 10 * 10);
    }

    /* Leaf H */
    char tempLeafH[5] = {0};
    uint16_t tempLH = 0;

    memcpy(tempLeafH, rawData_Leaf + 10, 4);
    tempLH = (uint16_t)strtol(tempLeafH, NULL, 16); // 356 (35.6)
    sprintf(leafH, "%d.%01d", tempLH / 10, tempLH % 10);
  }

  DEBUG("----------------------------------\r\n");
  return (funcptr)fsm_checkModule;
}


funcptr fsm_checkModule()
{
  static bool reInitFlag = false;
  if(exs62_ready()) {
    DEBUG(">> CatM module <<\r\n");
    isCatM = true;
    moduleFlag = true;

    if(exs62_setHttp()) {
      DEBUG("CatM setting OK\r\n");
    }
    else {
      DEBUG("CatM setting Fail\r\n");
      moduleFlag = false;
      if(!reInitFlag) {
        reInitFlag = true;
        NBIOT_PowerOff();
        vTaskDelay(pdMS_TO_TICKS(1000));
        NBIOT_PowerOn();
        vTaskDelay(pdMS_TO_TICKS(10000));
        return (funcptr)fsm_checkModule;
      }
    }
  }
  else {
    DEBUG(">> nblox module <<\r\n");
    sara_ready();

    if(sara_setHttp()) {
      DEBUG("nblox setting OK\r\n");
      moduleFlag = true;
    }
    else {
      moduleFlag = false;
      DEBUG("nblox setting Fail\r\n");
      if(!reInitFlag) {
        reInitFlag = true;
        NBIOT_PowerOff();
        vTaskDelay(pdMS_TO_TICKS(1000));
        return (funcptr)fsm_checkModule;
      }
    }
  }

  return (funcptr)nb_waitIP;
}

funcptr nb_waitIP() {
  static uint32_t waitIPCount = 0;
  if(isCatM)
  {
   if(exs62_connectNetwork()) {
      noConnectFlag = false;
    } else {
      DEBUG("Connect network fail\r\n");
      noConnectFlag = true;
    }

    return (funcptr)nb_getSignalPower;
  }
  else
  {
    DEBUG("(UART) AT+CGDCONT?\r\n");
    if(sara_checkIP()) {
      waitIPCount = 0;
      return (funcptr)nb_getSignalPower;
    }
    else {
      waitIPCount++;
      vTaskDelay(pdMS_TO_TICKS(1000));
      if(waitIPCount >= 300) {
        waitIPCount = 0;
        noConnectFlag = true;
        DEBUG("Doesn't get IP.\r\n");     
        return (funcptr)nb_getSignalPower;
      }
    }
  }

  return (funcptr)nb_waitIP;
}

funcptr nb_getSignalPower() {
  char msg[100] = {0};
  memset(RSSI, 0, sizeof(RSSI));

  if(noConnectFlag) {
    RSSI[0] = '0';
    DEBUG("Module no network, rssi = 0\r\n");
    return (funcptr)nb_writeNewDataInBuffer;
  }

  /* Get power */
  if(!isCatM) { // Ublox
    HAL_Delay(50);
    DEBUG("(UART) AT+CESQ\r\n");
    sara_getRSSI(RSSI);
  }else { // Cat M
    exs62_getRSSI(RSSI);
  }

  DEBUG("RSSI = ");DEBUG(RSSI);DEBUG("\r\n");

  return (funcptr)nb_writeNewDataInBuffer;
}

funcptr nb_writeNewDataInBuffer() {
  sprintf(newData, "{\"device_id\":%lu,\"hash\":\"%s\",\"data\":{\"st\":%s,\"sm\":%s,\"sec\":%s,\"sph\":%s,\"at\":%s,\"ah\":%s,\"dp\":%s,\"spar\":%s,\"wdep\":%s,\"sr\":%s,\"ws\":%s,\"lt\":%s,\"lh\":%s,\"it\":%s,\"sv\":%s,\"volt\":%s,\"rssi\":%s,\"snr\":%s},\"gts\":%lu}",
            pMAC,
            pHash,
            soilTemp,
            soilVWC,
            soilEC,
            soilPH,
            airT,
            airH,
            "0.0",      // DewPoint
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
            tempTimestamp);
  
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

  return (funcptr)nb_checkEEPROM;
}

static uint32_t dataPos = 1;
funcptr nb_checkEEPROM() {
  // Check eeprom have data or not
  uint8_t eepromData[payloadLen + 1] = {0};
  uint32_t addr = eepromFlag.nextSendAddress + (dataPos - 1) * payloadLen;
  if(addr >= MAX_EEPROM_SIZE) {
    DEBUG("Over max size, eeprom addr turn to 384.\r\n");
    addr = addr - MAX_EEPROM_SIZE + dataFirstAddress;
  }
  bool sts = parLoad(eepromData, addr, payloadLen);
  DEBUG(">>NextSendPos : ");writeUint32(DEBUG_USART, addr);DEBUG("\r\n");


  if(!sts || eepromData[0] == 0xFF || amountOfData >= 20) {
    DEBUG("End, Amount of data : ");writeUint32(DEBUG_USART, amountOfData);DEBUG("\r\n");
    return (funcptr)nb_writeDataInNBIOT;
  }
  else if(strnlen((char *)eepromData, payloadLen) <= 150) {
    DEBUG("This data have problem, jump it.\r\n");
    dataPos++;
    return (funcptr)nb_checkEEPROM;
  }
  else {
      DEBUG("Get data in eeprom.\r\n");
      strcat(readySendData, ",");
      strcat(readySendData, (char *)eepromData);
      amountOfData++;
      dataPos++;

      return (funcptr)nb_checkEEPROM;
  }
}

funcptr nb_writeDataInNBIOT() {

  char tempArray[12000] = {0};
  int len = sprintf(tempArray, "{\"num\":%lu,\"records\":[%s]}",
            amountOfData,
            readySendData
            );

  if(!isCatM) {
    if(sara_writeInFileSystem(tempArray, len)) DEBUG("Write in filesystem OK\r\n");
    else                                       DEBUG("Fail to write data in filesystem\r\n");
  } else {
    if(!exs62_preparePayload_Connect(len)) DEBUG("exs62_preparePayload_Connect fail\r\n");
    if(!exs62_http_send(tempArray, len))   DEBUG("exs62_http_send fail\r\n");
    return (funcptr)catM_getResponse;
  }

  return (funcptr)nb_sendData;
}

funcptr nb_sendData() {
  int result = sara_http_send();
  if(result == 0) { // Success
    DEBUG("Send is OK\r\n");
    vTaskDelay(pdMS_TO_TICKS(2000));
    return (funcptr)nb_getResponse;
  }
  else if(result == 1) { // 422
    DEBUG("Get 422 status\r\n");
    eeprom_ClearData();
  }
  else { // unknow error
    DEBUG("Save in eeprom.\r\n");
    eeprom_saveData_and_jumpNextPos((uint8_t*)newData);
    return (funcptr)fsm_sleep;
  }

  return (funcptr)fsm_sleep;
}

funcptr nb_getResponse() 
{
  char msg[800 + 1] = {0};
  int len = 0;
  lpuart1DataLength = 0;

  /* 讀取一部份, 判斷是否為200 */
  DEBUG("(UART) AT+URDBLOCK=\"rp1\",0,40\r\n");
  writeString(LPUART1, "AT+URDBLOCK=\"rp1\",0,40\r\n");
  vTaskDelay(pdMS_TO_TICKS(100));
  len = lpuart1DataLength;
  receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
  if (inputStringLpuart1.ready == 1) {
    inputStringLpuart1.ready = 0;
  }

  char *partMsg  = strstr(inputStringLpuart1.string, "200");
  if(partMsg == NULL) { // Not 200 or 422
    DEBUG("Result : FAIL!\r\n");

    DEBUG("This data store at : ");writeUint32(DEBUG_USART, eepromFlag.nextSaveAddress);
    DEBUG("\r\n");
    parSave((uint8_t*)newData, eepromFlag.nextSaveAddress, payloadLen);
    changeSaveAddress(eepromFlag.nextSaveAddress + payloadLen);
    DEBUG("Next store at : ");writeUint32(DEBUG_USART, eepromFlag.nextSaveAddress);
    DEBUG("\r\n");
    return (funcptr)fsm_sleep;
  }

  memset(msg, 0, sizeof(msg));
  DEBUG("(UART) AT+URDFILE=\"rp1\"\r\n");
  writeString(LPUART1, "AT+URDFILE=\"rp1\"\r\n");
  vTaskDelay(pdMS_TO_TICKS(3000));
  len = lpuart1DataLength;
  receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
  if (inputStringLpuart1.ready == 1) {
    inputStringLpuart1.ready = 0;
    memcpy(msg, inputStringLpuart1.string, len);
  }

  DEBUG("=============== MSG ===================\r\n");
  writeData(DEBUG_USART, msg, len);
  DEBUG("=============== === ===================\r\n");

  char *str = strstr(msg, "\"ack\":");
  if(str != NULL && len < 1000) { /* Get success response */
    DEBUG(" Get success ACK\r\n");
    sendOKFlag = true;
    vTaskDelay(pdMS_TO_TICKS(1000));
    eeprom_ClearData();

    /* TODO : Get resendTime and rtc time */
    uint32_t startPos = str - msg;
    char resendTime[10] = {0};
    char timestemp[15] = {0};

    int i = 0;
    while((msg[startPos + 7 + i] != '|') && i < 10)
    {
      resendTime[i] = msg[startPos + 7 + i];
      i++;
    }
    /* Now pos is '|' */
    memcpy(timestemp, msg + startPos + 7 + i + 1, 12);
    DEBUG("New ResendTime : ");DEBUG(resendTime);DEBUG(", time : ");DEBUG(timestemp);DEBUG("\r\n");

    /* Resend time */
    uint32_t getResendTime = (uint32_t)strtol(resendTime, NULL, 10);
    if(getResendTime != pResendTime && getResendTime > 60) {
      DEBUG("Change RT from ");writeUint32(DEBUG_USART, pResendTime);
      DEBUG(" to ");writeUint32(DEBUG_USART, getResendTime);
      DEBUG("\r\n");
      nodeManager_changeResendTime((uint64_t)getResendTime);
      DEBUG("Now RT = ");writeUint32(DEBUG_USART, pResendTime);DEBUG("\r\n");
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
          if(year >= 30) {
            DEBUG("Error : Ack's time is over 2030.\r\n");
          }
          else {
            rtc.setTime(hour, minute, second);
            rtc.setDate(year, month, day);
            DEBUG("Fix rtc time ok.\r\n");
          }
      }
    }
  }
  else {
    DEBUG("No success response, save in eeprom.\r\n");
    eeprom_saveData_and_jumpNextPos((uint8_t*)newData);
  }

  return (funcptr)fsm_sleep;
}

funcptr catM_getResponse() {
  char msg[800 + 1] = {0};
  int len = 0;

  /* 這裡需要出分出200、422和其他 */
  /* If http status is 422 */
  if(exs62_getResponse(3, "HTTP/1.1 422")) {
    DEBUG("Result : FAIL!State 422.\r\n");
    
    /* 關閉連線 */
    sendCmd("AT^SISC=6\r\n");
    if(!exs62_getResponse(3, "OK"))
    {
      DEBUG("Close connection fail!\r\n");
    }
    else
      DEBUG("Close connection OK\r\n");

    eeprom_ClearData();

    return (funcptr)fsm_sleep;
  }

  /* If http state is 200 or other */
  char *part = strstr(inputStringLpuart1.string, "^SISR: 6,1");
  if(part == NULL) { /* Not 200 */
    DEBUG("Result : Fail!\r\n");

    DEBUG("*-*-*-*-*-*-*-*-*-*-*\r\n");
    DEBUG(inputStringLpuart1.string);
    DEBUG("\r\n*-*-*-*-*-*-*-*-*-*-*\r\n");

    /* 關閉連線 */
    sendCmd("AT^SISC=6\r\n");
    if(!exs62_getResponse(3, "OK"))
    {
      DEBUG("Close connection fail!\r\n");
    }
    else
    {
      DEBUG("Close connection OK\r\n");
    }

    //儲存最新一筆資料
    DEBUG("Save in eeprom.\r\n");
    eeprom_saveData_and_jumpNextPos((uint8_t*)newData);

    return (funcptr)fsm_sleep;
  }

  /* State 200 */
  DEBUG("Result :OK!\r\n");
  sendOKFlag = true;
  vTaskDelay(pdMS_TO_TICKS(1000));

  eeprom_ClearData();

  sendCmd("AT^SISR=6,1500\r\n");
  exs62_getResponse(3, "\"ack\"");

  DEBUG("=================\r\n");
  DEBUG(inputStringLpuart1.string);
  DEBUG("\r\n=================\r\n");

  /* Get resendTime and rtc time */
  part = strstr(inputStringLpuart1.string, "\"ack\":");
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
    DEBUG("Now RT = ");DEBUG_uint(pResendTime);DEBUG("\r\n");
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
  sendCmd("AT^SISC=6\r\n");
  if(!exs62_getResponse(3, "OK"))
  {
    DEBUG("Close connection fail!\r\n");

    sendCmd("AT^SISC=6\r\n");
    if(!exs62_getResponse(3, "OK"))
      DEBUG("Close connection fail again!\r\n");
  }
  else
  {
    DEBUG("Clsoe connection OK\r\n");
  }

  return (funcptr)fsm_sleep;
}

funcptr catM_Connect_And_Send() {
  char tempArray[12000] = {0};
  int newPayloadLen = sprintf(tempArray, "{\"num\":%lu,\"records\":[%s]}",
            amountOfData,
            readySendData
            );

  // 設定資料長度
  char cmd[100] = {0};
  sprintf(cmd, "AT^SISS=6,\"hcContLen\",\"%lu\"\r\n", newPayloadLen);
  sendCmd(cmd);
  if(!exs62_getResponse(3, "OK"))
  {
    DEBUG("Setting hcContLen fail!\r\n");
  }
  else
    DEBUG("Setting hcContLen OK\r\n");

  // 開啟連線 + 寫資料 + 傳送
  sendCmd("AT^SISO=6\r\n");
  if(!exs62_getResponse(5, "^SISW: 6,1"))
  {
    DEBUG("AT^SISO=6 fail!\r\n");
    sendCmd("AT^SISO=6\r\n");
    if(!exs62_getResponse(5, "^SISW: 6,1"))
    {
      DEBUG("AT^SISO=6 fail again!\r\n");
      NBIOT_PowerOn();
      vTaskDelay(pdMS_TO_TICKS(500));
      return (funcptr)fsm_idle;
    }
  }
  else
  {
    DEBUG("AT^SISO OK\r\n");
  }
  memset(inputStringLpuart1.string, 0, sizeof(inputStringLpuart1.string));
  memset(cmd, 0, sizeof(cmd));

  DEBUG("LEN = ");writeUint32(DEBUG_USART, newPayloadLen);DEBUG("\r\n");
  DEBUG("Ready to write data ...\r\n");
  if(newPayloadLen <= 1500)
  {
    sprintf(cmd, "AT^SISW=6,%lu\r\n", newPayloadLen);
    sendCmd(cmd);
    HAL_Delay(50);
    sendCmd(newData);
  }
  else
  {
    char * pData = tempArray;
    uint8_t count = newPayloadLen / 1500;
    uint32_t len = newPayloadLen;
    DEBUG("Count = ");writeUint32(DEBUG_USART, count);DEBUG("\r\n");
    for(int i = 0; i < count; i++)
    {
      sendCmd("AT^SISW=6,1500\r\n");
      HAL_Delay(50);
      writeData(LPUART1, pData, 1500);
      pData += 1500;
      len -= 1500;

      if(exs62_getResponse(3, "SISW: 6,1"))
      {
        DEBUG("Send OK\r\n");
        continue;
      }
      else
      {
        DEBUG("Send Fail\r\n");
        break;
      }
    }

    sprintf(cmd, "AT^SISW=6,%lu\r\n", len);
    sendCmd(cmd);
    HAL_Delay(50);
    writeData(LPUART1, pData, len);
  }

  return (funcptr)nb_getResponse;
}

funcptr fsm_sleep() {

  DEBUG(">>fsm_sleep<<\r\n");
  dog.reload();

  if(!isCatM) {
    sara_poweroff();
  }

  DEBUG("Sleep time : ");writeUint32(DEBUG_USART, pResendTime);DEBUG("\r\n");
  HAL_PWREx_EnterSHUTDOWNMode();
  return 0;
}

//-------------------private function------------------------------
static void voltToTemperature(float tempVolt, char * tempStr)
{
  float originV = ADC_calculateTemperature(tempVolt);
  float tmpVal  = originV;
  int   tmpInt1 = tmpVal;
  float tmpFrac = tmpVal - tmpInt1;
  int   tmpInt2 = std::trunc(tmpFrac * 10);
  if (tmpInt2 < 0) tmpInt2 *= -1;
  sprintf (tempStr, "%d.%01d", tmpInt1, tmpInt2);
}

static void floatToString(float value, char * str, int dot) {
  float tmpVal = value;
  int tmpInt1 = tmpVal;
  float tmpFrac = tmpVal - tmpInt1;
  int tmpInt2 = std::trunc(tmpFrac * dot);

  sprintf (str, "%d.%d", tmpInt1, tmpInt2);
}

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
  if(amountOfData > 1) {
    uint32_t finalAddr = 0;
    for(int i = 1; i < dataPos; i++) {
      uint32_t addr = eepromFlag.nextSendAddress + (i - 1) * payloadLen;
      if(addr >= MAX_EEPROM_SIZE) addr = addr - MAX_EEPROM_SIZE + dataFirstAddress;
      parSave(nonData, addr, payloadLen);
      DEBUG("Delete data at : ");writeUint32(DEBUG_USART, addr);DEBUG("\r\n");
      finalAddr = addr + payloadLen;
    }

    changeSendAddress(finalAddr);
  }
}

//-----------------------------------------------------------------
