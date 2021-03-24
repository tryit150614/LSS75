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
#include "lora_rylr890.h"
#include "nodeManager.h"
#include "rng.h"
#include "eeprom.h"
#include "st_rtc.h"
#include "st_watchdog.h"

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
extern unsigned int lpuart1DataLength;

extern uint16_t adc1DmaBuffer[DMA1_ADC_BUFFER_SIZE];
extern volatile uint32_t pResendTime;
extern volatile uint32_t pMAC;
extern char pHash[9];

extern volatile bool rs485Flag;
extern volatile bool getDataFlag;
extern volatile bool nbiotFindIPFlag;
extern volatile bool nbiotGetIPFlag;
extern volatile bool getOKFlag;
extern volatile bool getErrorFlag;
bool noConnectFlag = false;

extern RNG_HandleTypeDef hrng;
extern EEPROMFlag eepromFlag;

extern Rtc rtc;

/* Payload */
uint32_t protocolID = 30;
char     soilTemp[20]      = "0.00";
char     soilVWC [20]      = "0";
char     soilEC  [20]      = "0.00";
char     soilPH  [20]      = "0.00";
char     airT [20]         = "0.00";
char     airH[20]          = "0.00";
char     solra [10]           = "0";
char     waterDepth [10]      = "0";
char     sunRadiation[10]     = "0";
char     windSpeed [10]       = "0";
char     leafH  [10]          = "0";
char     leafT  [10]          = "0";
char     newData[256+1]           = {0};
char     readySendData[10500+1]   = {0};
uint32_t amountOfData     = 0;
uint32_t tempTimestamp    = 0;

char connectIP[] = "\"35.187.150.128\"";
char connectPort[] = "80";
char connectPath[100] = "\"/api/6th-nbiot\"";
char signalPower[3] = {0};
//////////////////////////////

struct FSMAttributes fsmAttributes;
Watchdog dog;

static uint8_t nonData[payloadLen + 1]   = {0};
static char rs485Data_Soil[11 * 2 + 1]   = {0};
static char rs485Data_Soil_2[11 * 2 + 1] = {0};
static char rs485Data_PH[9 * 2 + 1]      = {0};
static char rs485Data_PH_2[7 * 2 + 1]    = {0};
static char rs485Data_Air[11 * 2 + 1]    = {0};
static char rawData_Solar[7 * 2 + 1]     = {0};
static char rawData_Waterdepth[7 * 2 + 1]= {0};
static char rawData_SR[7 * 2 + 1]        = {0};
static char rawData_WS[7 * 2 + 1]        = {0};
static char rawData_WS_2[7 * 2 + 1]      = {0};
static char rawData_Leaf[9 * 2 + 1]      = {0};
static char batV[10] = {0}, solarV[10]   = {0}, innerT[10] = {0};
static uint8_t loraResendCount  = 0;
// static uint8_t sendSuccessCount = 0;
static bool    eepromMode       = false;
// Declare private functions---------
static void voltToTemperature(float tempVolt, char * tempStr);
static void floatToString(float value, char * str, int dot);
static void transmitRS485(uint8_t * deviceNumber);
static void getRS485Data(char * deviceData, int receiveDataLen);
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
  return (funcptr)fsm_getADCValue;
}

funcptr fsm_getADCValue()
{
  // writeString(DEBUG_USART, ">>fsm_getADCValue<<\r\n");
  
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
    floatToString(tempVolt[1], solarV, 100);
    //-----------Temperature---------//
    voltToTemperature(tempVolt[2], innerT);

    /* Direct battery voltage, if < 2.8V, just sleep. */
    writeString(DEBUG_USART,"BatteryV : ");writeString(DEBUG_USART, batV);writeString(DEBUG_USART,"\r\n");
    if (tempVolt[0] < 2.900000 && tempVolt[0] >= 0.00000)
    {

      writeString(DEBUG_USART,"Keep sleep ...\r\n");
      return (funcptr)fsm_sleep;
    }

    tempTimestamp = getUnixTimestamp();
    getDataFlag = false;

  return (funcptr)fsm_getRS485Data;
}

funcptr fsm_getRS485Data()
{
  writeString(DEBUG_USART, ">>fsm_getRS485Data<<\r\n");
  memset(rs485Data_Soil, 0, sizeof(rs485Data_Soil));
  memset(rs485Data_Soil_2, 0, sizeof(rs485Data_Soil_2));
  memset(rs485Data_PH, 0, sizeof(rs485Data_PH));
  memset(rs485Data_PH_2, 0, sizeof(rs485Data_PH_2));
  memset(rs485Data_Air, 0, sizeof(rs485Data_Air));
  memset(rawData_Solar, 0, sizeof(rawData_Solar));
  memset(rawData_Waterdepth, 0, sizeof(rawData_Waterdepth));
  memset(rawData_SR, 0, sizeof(rawData_SR));
  memset(rawData_WS, 0, sizeof(rawData_WS));
  memset(rawData_WS_2, 0, sizeof(rawData_WS_2));
  memset(rawData_Leaf, 0, sizeof(rawData_Leaf));

  uint8_t soilSensor_array[8]       = {0x01,0x04,0x00,0x00,0x00,0x03,0xB0,0x0B};
  uint8_t PHSensor_array[8]         = {0x02,0x04,0x00,0x00,0x00,0x02,0x71,0xF8};
  uint8_t soilSensor_2_array[8]     = {0x03,0x03,0x00,0x00,0x00,0x03,0x04,0x29}; /* Laytion sensor */
  uint8_t airSensor_array[8]        = {0x04,0x04,0x00,0x00,0x00,0x03,0xB0,0x5E};
  uint8_t PHSensor_2_array[8]       = {0x05,0x03,0x00,0x00,0x00,0x01,0x85,0x8E}; /* Laytion sensor */
  uint8_t SolarSensor_array[8]      = {0x06,0x03,0x00,0x00,0x00,0x01,0x85,0xBD}; /* Laytion sensor */
  uint8_t waterDepthSensor_array[8] = {0x07,0x03,0x00,0x00,0x00,0x01,0x84,0x6C}; /* Laytion sensor */
  uint8_t sunRadiation_array[8]     = {0x08,0x04,0x00,0x00,0x00,0x01,0x31,0x53}; /* Infwin sensor */
  uint8_t windSpeed_array[8]        = {0x10,0x03,0x00,0x00,0x00,0x01,0x87,0x4B}; /* PRS sensor */
  uint8_t windSpeed_array2[8]       = {0x19,0x03,0x00,0x00,0x00,0x01,0x87,0xD2}; /* Laytion sensor */
  uint8_t leaf_array[8]             = {0x20,0x03,0x00,0x00,0x00,0x01,0x87,0x4B}; 

  powerOnRS485Sensor();
  rs485Flag = true;
  vTaskDelay(pdMS_TO_TICKS(10000));
  rs485Flag = false;
  memset(usart3Buf.bufferPtr, 0, usart3DataSize);
  ringBuffer_resetBuffer(&usart3Buf);

  transmitRS485(PHSensor_array);
  getRS485Data(rs485Data_PH, 9);
  transmitRS485(soilSensor_2_array);
  getRS485Data(rs485Data_Soil_2, 11);
  transmitRS485(airSensor_array);
  getRS485Data(rs485Data_Air, 11);
  transmitRS485(PHSensor_2_array);
  getRS485Data(rs485Data_PH_2, 7);
  transmitRS485(soilSensor_array);
  getRS485Data(rs485Data_Soil, 11);
  transmitRS485(SolarSensor_array);
  getRS485Data(rawData_Solar, 7);
  transmitRS485(waterDepthSensor_array);
  getRS485Data(rawData_Waterdepth, 7);
  transmitRS485(sunRadiation_array);
  getRS485Data(rawData_SR, 7);
  transmitRS485(windSpeed_array);
  getRS485Data(rawData_WS, 7);
  transmitRS485(windSpeed_array2);
  getRS485Data(rawData_WS_2, 7);
  transmitRS485(leaf_array);
  getRS485Data(rawData_Leaf, 9);

  powerOffRS485Sensor();

  if(strnlen(rs485Data_Soil, sizeof(rs485Data_Soil)) == 0){
    writeString(DEBUG_USART, "Soil senser : N/A\r\n");
    memset(soilTemp, 0, 20);
    memset(soilVWC, 0, 20);
    memset(soilEC, 0, 20);
    memcpy(soilTemp, "0.00", 4);
    memcpy(soilVWC, "0", 1);
    memcpy(soilEC, "0.000", 5);
  }
  else {
    writeString(DEBUG_USART, "Soil sensor data : ");
    writeString(DEBUG_USART, rs485Data_Soil);
    writeString(DEBUG_USART, "\r\n");

    /* Soil temp */
    memset(soilTemp, 0, sizeof(soilTemp));
    char tempSoilTemp[20] = {0};
    uint32_t tempST = 0;

    memcpy(tempSoilTemp, rs485Data_Soil + 6, 4);
    tempST = (uint32_t)strtol(tempSoilTemp, NULL, 16); // need/100 ex:1794 -> 17.94
    if(tempST <= 20000) { // temp <= 200.00 C
      sprintf(soilTemp, "%lu.%02lu", tempST / 100, tempST % 100);
    }
    else {
      sprintf(soilTemp, "-%lu.%02lu", (65536 - tempST + 1) / 100, (65536 - tempST + 1) % 100);
    }
    DEBUG("SoilTemp : ");DEBUG(soilTemp);DEBUG("\r\n");

    /* Soil VMC */
    memset(soilVWC, 0, sizeof(soilVWC));
    char tempVMCStr[20] = {0};
    uint32_t tempVMC = 0;

    memcpy(tempVMCStr, rs485Data_Soil + 10, 4);
    tempVMC = (uint32_t)strtol(tempVMCStr, NULL, 16) / 100;
    sprintf(soilVWC, "%lu", tempVMC);
    DEBUG("SoilVWC : ");DEBUG(soilVWC);DEBUG("\r\n");

    /* Soil EC */
    memset(soilEC, 0, sizeof(soilEC));
    char tempECStr[20] = {0};
    uint32_t tempEC = 0;

    memcpy(tempECStr, rs485Data_Soil + 14, 4);
    tempEC = (uint32_t)strtol(tempECStr, NULL, 16);
    sprintf(soilEC, "%lu.%03lu", tempEC / 1000, tempEC % 1000);
    DEBUG("SoilEC : ");DEBUG(soilEC);DEBUG("\r\n");
  }

  if(strnlen(rs485Data_PH, sizeof(rs485Data_PH)) == 0){
    writeString(DEBUG_USART, "PH senser : N/A\r\n");
    memset(soilPH, 0, 20);
    memcpy(soilPH, "0.00", 4);
  }
  else {
    // writeString(DEBUG_USART, "PH sensor data : ");
    // writeString(DEBUG_USART, rs485Data_PH);
    // writeString(DEBUG_USART, "\r\n");

    /* PH */
    memset(soilPH, 0, sizeof(soilPH));
    char tempSoilPH[20] = {0};
    uint32_t tempPH = 0;

    memcpy(tempSoilPH, rs485Data_PH + 10, 4);
    tempPH = (uint32_t)strtol(tempSoilPH, NULL, 16); // 700 -> 7.00
    sprintf(soilPH, "%lu.%02lu", tempPH / 100, tempPH % 100);
    DEBUG("Soil PH : ");DEBUG(soilPH);DEBUG("\r\n");
  }
  
  if(strnlen(rs485Data_Soil_2, sizeof(rs485Data_Soil_2)) == 0){
    writeString(DEBUG_USART, "Soil_2 senser : N/A\r\n");
  }
  else {
    writeString(DEBUG_USART, "Soil_2 sensor data : ");
    writeString(DEBUG_USART, rs485Data_Soil_2);
    writeString(DEBUG_USART, "\r\n");

    /* Soil temp */
    memset(soilTemp, 0, sizeof(soilTemp));
    char tempSoilTemp[20] = {0};
    uint32_t tempST = 0;

    memcpy(tempSoilTemp, rs485Data_Soil_2 + 6, 4);
    tempST = (uint32_t)strtol(tempSoilTemp, NULL, 16);
    if(tempST < 10000) {
      // 235 -> 23.50
      sprintf(soilTemp, "%lu.%02lu", tempST / 10, tempST % 10 * 10);
      DEBUG("soilTemp = ");DEBUG(soilTemp);DEBUG("\r\n");
    }
    else {
      tempST = (0xffff - tempST + 1); // 35
      sprintf(soilTemp, "-%lu.%02lu", tempST / 10, tempST % 10 * 10); // 35 -> -3.50
      DEBUG("soilTemp = ");DEBUG(soilTemp);DEBUG("\r\n");
    }

    /* Soil VMC */
    memset(soilVWC, 0, sizeof(soilVWC));
    char tempVMCStr[20] = {0};
    uint32_t tempVMC = 0;

    memcpy(tempVMCStr, rs485Data_Soil_2 + 10, 4);
    tempVMC = (uint32_t)strtol(tempVMCStr, NULL, 16) / 10;
    sprintf(soilVWC, "%lu", tempVMC);
    DEBUG("soilVWC = ");DEBUG(soilVWC);DEBUG("\r\n");

    /* Soil EC */
    memset(soilEC, 0, sizeof(soilEC));
    char tempECStr[20] = {0};
    uint32_t tempEC = 0;

    memcpy(tempECStr, rs485Data_Soil_2 + 14, 4);
    tempEC = (uint32_t)strtol(tempECStr, NULL, 16);
    sprintf(soilEC, "%lu.%02lu", tempEC / 100, tempEC % 100);
    DEBUG("soilEC = ");DEBUG(soilEC);DEBUG("\r\n");
  }

  if(strnlen(rs485Data_Air, sizeof(rs485Data_Air)) == 0){
    writeString(DEBUG_USART, "Air senser : N/A\r\n");
    memset(airT, 0, sizeof(airT));
    memset(airH, 0, sizeof(airH));
    memcpy(airT, "0.00", 4);
    memcpy(airH, "0.00", 4);
  }
  else {
    writeString(DEBUG_USART, "Air sensor data : ");
    writeString(DEBUG_USART, rs485Data_Air);
    writeString(DEBUG_USART, "\r\n");

    /* Air temp */
    memset(airT, 0, sizeof(airT));
    char tempAirT[5] = {0};
    uint16_t tempAT = 0;

    memcpy(tempAirT, rs485Data_Air + 6, 4);
    tempAT = (uint16_t)strtol(tempAirT, NULL, 16); // 3000 (30.00)
    if(tempAT <= 20000) { // temp <= 200.00 C
      sprintf(airT, "%lu.%02lu", tempAT / 100, tempAT % 100);
    }
    else {
      sprintf(airT, "%lu.%02lu", (65536 - tempAT) / 100, (65536 - tempAT) % 100);
    }
    DEBUG("Air T : ");DEBUG(airT);DEBUG("\r\n");

    /* Air H */
    memset(airH, 0, sizeof(airH));
    char tempAirHStr[5] = {0};
    uint16_t tempAH = 0;

    memcpy(tempAirHStr, rs485Data_Air + 10, 4);
    tempAH = (uint16_t)strtol(tempAirHStr, NULL, 16); // 3000 (30.00%)
    sprintf(airH, "%lu.%02lu", tempAH / 100, tempAH % 100);
    DEBUG("Air H : ");DEBUG(airH);DEBUG("\r\n");
  }

  if(strnlen(rs485Data_PH_2, sizeof(rs485Data_PH_2)) == 0){
    writeString(DEBUG_USART, "PH_2 senser : N/A\r\n");
  }
  else {
    writeString(DEBUG_USART, "PH_2 sensor data : ");
    writeString(DEBUG_USART, rs485Data_PH_2);
    writeString(DEBUG_USART, "\r\n");

    /* PH */
    memset(soilPH, 0, sizeof(soilPH));
    char tempSoilPH[5] = {0};
    uint16_t tempPH = 0;

    memcpy(tempSoilPH, rs485Data_PH_2 + 6, 4);
    tempPH = (uint16_t)strtol(tempSoilPH, NULL, 16);
    sprintf(soilPH, "%lu.%02lu", tempPH / 100, tempPH % 100);
    DEBUG("PH : ");DEBUG(soilPH);DEBUG("\r\n");
  }

  /* Solra */
  if(strnlen(rawData_Solar, sizeof(rawData_Solar)) == 0){
    writeString(DEBUG_USART, "Solra senser : N/A\r\n");
    memset(solra, 0, sizeof(solra));
    memcpy(solra, "0.0", 3);
  }
  else {
    writeString(DEBUG_USART, "Solra sensor data : ");
    writeString(DEBUG_USART, rawData_Solar);
    writeString(DEBUG_USART, "\r\n");

    memset(solra, 0, sizeof(solra));
    memcpy(solra, rawData_Solar + 6, 4);
  }

  /* Water depth */ 
  if(strnlen(rawData_Waterdepth, sizeof(rawData_Waterdepth)) == 0){
    writeString(DEBUG_USART, "Water depth senser : N/A\r\n");
    memset(waterDepth, 0, sizeof(waterDepth));
    memcpy(waterDepth, "0.0", 3);
  }
  else {
    writeString(DEBUG_USART, "water depth sensor data : ");
    writeString(DEBUG_USART, rawData_Waterdepth);
    writeString(DEBUG_USART, "\r\n");

    memset(waterDepth, 0, sizeof(waterDepth));
    memcpy(waterDepth, rawData_Waterdepth + 6, 4);
  }

  /* Sun Radiation */
  if(strnlen(rawData_SR, sizeof(rawData_SR)) == 0){
    writeString(DEBUG_USART, "Sun Radiation senser : N/A\r\n");
    memset(sunRadiation, 0, sizeof(sunRadiation));
    memcpy(sunRadiation, "0.0", 3);
  }
  else {
    writeString(DEBUG_USART, "Sun Radiation sensor data : ");
    writeString(DEBUG_USART, rawData_SR);
    writeString(DEBUG_USART, "\r\n");

    memset(sunRadiation, 0, sizeof(sunRadiation));
    memcpy(sunRadiation, rawData_SR + 6, 4);
  }

  /* Wind Speed */
  if(strnlen(rawData_WS, sizeof(rawData_WS)) == 0){
    writeString(DEBUG_USART, "Wind speed senser : N/A\r\n");
    memset(windSpeed, 0, sizeof(windSpeed));
    memcpy(windSpeed, "0.0", 3);
  }
  else {
    writeString(DEBUG_USART, "Wind speed sensor data : ");
    writeString(DEBUG_USART, rawData_WS);
    writeString(DEBUG_USART, "\r\n");

    memset(windSpeed, 0, sizeof(windSpeed));
    memcpy(windSpeed, rawData_WS + 6, 4);
  }

  /* Wind Speed 2 */
  if(strnlen(rawData_WS_2, sizeof(rawData_WS_2)) == 0){
    writeString(DEBUG_USART, "Wind speed senser : N/A\r\n");
  }
  else {
    writeString(DEBUG_USART, "Wind speed sensor 2 data : ");
    writeString(DEBUG_USART, rawData_WS_2);
    writeString(DEBUG_USART, "\r\n");

    char tempStr[5] = {0};
    memcpy(tempStr, rawData_WS_2 + 6, 4);
    uint32_t value = strtol(tempStr, NULL, 16) / 10;

    memset(windSpeed, 0, sizeof(windSpeed));
    sprintf(windSpeed, "%x", value);
  }
  
  /* Leaf T&H */
  if(strnlen(rawData_Leaf, sizeof(rawData_Leaf)) == 0){
    writeString(DEBUG_USART, "Leaf T&H senser : N/A\r\n");
    memset(leafT, 0, sizeof(leafT));
    memset(leafH, 0, sizeof(leafH));
    memcpy(leafT, "0.00", 4);
    memcpy(leafH, "0", 1);
  }
  else {
    writeString(DEBUG_USART, "Leaf T&H sensor data : ");
    writeString(DEBUG_USART, rawData_Leaf);
    writeString(DEBUG_USART, "\r\n");

    /* Leaf temp */
    memset(leafT, 0, sizeof(leafT));
    char tempLeafT[5] = {0};
    uint16_t tempLT = 0;

    memcpy(tempLeafT, rawData_Leaf + 6, 4);
    tempLT = (uint16_t)strtol(tempLeafT, NULL, 16);
    if(tempLT <= 10000) { 
      sprintf(leafT, "%lu.%02lu", tempLT / 100, tempLT % 100);
    }
    else {
      sprintf(leafT, "-%lu.%02lu", (65536 - tempLT + 1) / 10, (65536 - tempLT + 1) % 100 * 10);
    }

    /* Leaf H */
    memset(leafH, 0, sizeof(leafH));
    char tempLeafH[5] = {0};
    uint16_t tempLH = 0;

    memcpy(tempLeafH, rawData_Leaf + 10, 4);
    tempLH = (uint16_t)strtol(tempLeafH, NULL, 16) / 10; // 356 (35.6) -> 35 (35)
    sprintf(leafH, "%d", tempLH);
  }

  return (funcptr)nb_idle;
}

funcptr nb_idle()
{
  NBIOT_PowerOn();
  nbiotFindIPFlag = true;
  LL_GPIO_ResetOutputPin(pNBIOT_PWR, ioNBIOT_PWR);
  vTaskDelay(pdMS_TO_TICKS(3200));
  LL_GPIO_SetOutputPin(pNBIOT_PWR, ioNBIOT_PWR);
  vTaskDelay(pdMS_TO_TICKS(1300));


  return (funcptr)nb_waitIP;
}

funcptr nb_getSignalPower() {
  DEBUG("Get signal power\r\n");

  char msg[100] = {0};
  /* Get power */
  writeString(DEBUG_USART, "(UART) AT+CSQ\r\n");
  writeString(LPUART1, "AT+CSQ\r\n");
  uint32_t start = HAL_GetTick();
  uint32_t current = HAL_GetTick();
  const uint32_t timeout = 1000;
  while ((current - start) < timeout)
  {
    dog.reload();
    current = HAL_GetTick();
    receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
    if (inputStringLpuart1.ready == 1)
    {
      inputStringLpuart1.ready = 0;
      strcat(msg, inputStringLpuart1.string);
    }
  }

  signalPower[0] = msg[15];
  if(msg[16] != ',') signalPower[1] = msg[16];

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
            "0.00",      // DewPoint
            solra,
            waterDepth,
            sunRadiation,
            windSpeed,
            leafT,
            leafH,
            innerT,
            solarV,
            batV,
            signalPower,
            "0",         // SNR
            tempTimestamp);
  
  strcat(readySendData, newData);
  amountOfData = 1;

  if(noConnectFlag) {
    DEBUG("Save in eeprom.\r\n");
    writeString(DEBUG_USART, "This data store at : ");writeUint32(DEBUG_USART, eepromFlag.nextSaveAddress);
    writeString(DEBUG_USART, "\r\n");
    parSave((uint8_t*)newData, eepromFlag.nextSaveAddress, payloadLen);
    changeSaveAddress(eepromFlag.nextSaveAddress + payloadLen);
    writeString(DEBUG_USART, "Next store at : ");writeUint32(DEBUG_USART, eepromFlag.nextSaveAddress);
    writeString(DEBUG_USART, "\r\n");

    return (funcptr)fsm_sleep;
  }

  return (funcptr)nb_checkEEPROM;
}

funcptr nb_waitIP() {
  static int waitIPCount = 0;
  char msg[200] = {0};
  writeString(DEBUG_USART, "(UART) AT+CGDCONT?\r\n");
  writeString(LPUART1, "AT+CGDCONT?\r\n");

  uint32_t start = HAL_GetTick();
  uint32_t current = HAL_GetTick();
  const uint32_t timeout = 1000 * 2;
  while ((current - start) < timeout)
  {
    current = HAL_GetTick();
    dog.reload();
    receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
    if (inputStringLpuart1.ready == 1)
    {
      inputStringLpuart1.ready = 0;

      strcat(msg, inputStringLpuart1.string);
    }
  }

  if(memcmp(msg, ERROR_MSG_CGDCONT, ERROR_MSG_CGDCONT_TEXT_NUM) == 0) {
    waitIPCount++;
    if(waitIPCount >= 150) {
      waitIPCount = 0;
      noConnectFlag = true;
      DEBUG("Doesn't get IP.\r\n");     
      return (funcptr)nb_getSignalPower;
    }
  }
  else if(memcmp(msg, OK_MSG_CGDCONT, OK_MSG_CGDCONT_TEXT_NUM) == 0) {
    DEBUG("Wait IP OK\r\n");
    DEBUG("Spend time : ");writeInt(DEBUG_USART, waitIPCount * 2);DEBUG("\r\n");
    nbiotFindIPFlag = false;
    nbiotGetIPFlag = true;
    waitIPCount = 0;
    return (funcptr)nb_getSignalPower;
  }

  return (funcptr)nb_waitIP;
}

static uint32_t dataPos = 1;
funcptr nb_checkEEPROM() {
  // Check eeprom have data or not
  uint8_t eepromData[payloadLen + 1] = {0};
  uint32_t addr = eepromFlag.nextSendAddress + (dataPos - 1) * payloadLen;
  if(addr >= MAX_EEPROM_SIZE) {
    DEBUG("Over max size, eeprom addr turn to 256.\r\n");
    addr = addr - MAX_EEPROM_SIZE + dataFirstAddress;
  }
  bool sts = parLoad(eepromData, addr, payloadLen);
  writeString(DEBUG_USART, ">>NextSendAddress : ");writeUint32(DEBUG_USART, addr);writeString(DEBUG_USART, "\r\n");


  if(!sts || eepromData[0] == 0xFF || amountOfData >= 20) {
    writeString(DEBUG_USART, "Next addr is empty or amountOfData >= 20 or i2c read fail.\r\n");
    DEBUG("Amount of data : ");writeUint32(DEBUG_USART, amountOfData);DEBUG("\r\n");

    return (funcptr)nb_writeDataInNBIOT;
  }
  else if(strnlen((char *)eepromData, payloadLen) <= 150) {
    DEBUG("This data have problem, jump it.\r\n");
    dataPos++;
    return (funcptr)nb_checkEEPROM;
  }
  else {
      writeString(DEBUG_USART, "Get data in eeprom.\r\n");

      strcat(readySendData, ",");
      strcat(readySendData, (char *)eepromData);
      amountOfData++;
      dataPos++;

      return (funcptr)nb_checkEEPROM;
  }
}

funcptr nb_writeDataInNBIOT() {

  /* {\"num\":50,\"records\":[{....}]} */
  char tempArray[12000] = {0};
  int len = sprintf(tempArray, "{\"num\":%lu,\"records\":[%s]}",
            amountOfData,
            readySendData
            );
  DEBUG("Fianl array is ready.\r\n");

  /* Delete data and rp in file system */
  writeString(DEBUG_USART, "(UART) AT+UDELFILE=\"data1\"\r\n");
  writeString(LPUART1, "AT+UDELFILE=\"data1\"\r\n");
  uint32_t start = HAL_GetTick();
  uint32_t current = HAL_GetTick();
  uint32_t timeout = 2000;
  while ((current - start) < timeout)
  {
    dog.reload();
    current = HAL_GetTick();
    receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
    if (inputStringLpuart1.ready == 1)
    {
      inputStringLpuart1.ready = 0;
    }
  }
  
  /* Write data into file system */
  writeString(DEBUG_USART, "(UART) AT+UDWNFILE=\"data1\",");
  writeUint32(DEBUG_USART, len);
  writeString(DEBUG_USART, "\r\n");

  writeString(LPUART1, "AT+UDWNFILE=\"data1\",");
  writeUint32(LPUART1, len);
  writeString(LPUART1, "\r\n");

  start = HAL_GetTick();
  current = HAL_GetTick();
  while ((current - start) < timeout)
  {
    dog.reload();
    current = HAL_GetTick();
    receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
    if (inputStringLpuart1.ready == 1)
    {
      inputStringLpuart1.ready = 0;
      // strcat(msg, inputStringLpuart1.string);
    }
  }

  vTaskDelay(pdMS_TO_TICKS(2000));
  dog.reload();
  writeString(LPUART1, tempArray);  
  writeString(LPUART1, "\r\n");  

  start = HAL_GetTick();
  current = HAL_GetTick();
  while ((current - start) < timeout)
  {
    dog.reload();
    current = HAL_GetTick();
    receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
    if (inputStringLpuart1.ready == 1)
    {
      inputStringLpuart1.ready = 0;
      // strcat(msg, inputStringLpuart1.string);
    }
  }

  vTaskDelay(pdMS_TO_TICKS(1000));
  // memset(msg, 0, sizeof(msg));

  return (funcptr)nb_initHttp;
}

static uint8_t reInitCount = 0;
funcptr nb_initHttp()
{
  // DEBUG("Enter init http\r\n");
  vTaskDelay(pdMS_TO_TICKS(1000));

  uint8_t result = 0;
  /* Setting http server IP */
  writeString(LPUART1, "AT+UHTTP=0,0,");
  writeString(LPUART1, connectIP);
  writeString(LPUART1, "\r\n");

  writeString(DEBUG_USART, "(UART) AT+UHTTP=0,0,");
  writeString(DEBUG_USART, connectIP);
  writeString(DEBUG_USART, "\r\n");

  uint32_t start = HAL_GetTick();
  uint32_t current = HAL_GetTick();
  const uint32_t timeout = 5000;
  while ((current - start) < timeout)
  {
    dog.reload();
    current = HAL_GetTick();
    receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
    if (inputStringLpuart1.ready == 1)
    {
      inputStringLpuart1.ready = 0;
      char * detectStr = strstr(inputStringLpuart1.string, "OK");
      if(detectStr != NULL) {
        result++;
        break;
      }
    }
  }

  vTaskDelay(pdMS_TO_TICKS(500));
  dog.reload();

  /* Setting http server Port */
  writeString(LPUART1, "AT+UHTTP=0,5,");
  writeString(LPUART1, connectPort);
  writeString(LPUART1, "\r\n");

  writeString(DEBUG_USART, "(UART) AT+UHTTP=0,5,");
  writeString(DEBUG_USART, connectPort);
  writeString(DEBUG_USART, "\r\n");

  start = HAL_GetTick();
  current = HAL_GetTick();
  while ((current - start) < timeout)
  {
    dog.reload();
    current = HAL_GetTick();
    receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
    if (inputStringLpuart1.ready == 1)
    {
      inputStringLpuart1.ready = 0;
      char * detectStr = strstr(inputStringLpuart1.string, "OK");
      if(detectStr != NULL) {
        result++;
        break;
      }
    }
  }

  vTaskDelay(pdMS_TO_TICKS(500));

  /* Setting http server header */
  writeString(DEBUG_USART, "(UART) AT+UHTTP=0,9,\"0:Content-Type:application/json\"\r\n");
  writeString(LPUART1, "AT+UHTTP=0,9,\"0:Content-Type:application/json\"\r\n");
  start = HAL_GetTick();
  current = HAL_GetTick();
  while ((current - start) < timeout)
  {
    dog.reload();
    current = HAL_GetTick();
    receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
    if (inputStringLpuart1.ready == 1)
    {
      inputStringLpuart1.ready = 0;
      char * detectStr = strstr(inputStringLpuart1.string, "OK");
      if(detectStr != NULL) {
        result++;
        break;
      }
    }
  }

  vTaskDelay(pdMS_TO_TICKS(500));

  writeString(DEBUG_USART, "(UART) AT+UHTTP=0,9,\"1:Accept:application/json\"\r\n");
  writeString(LPUART1, "AT+UHTTP=0,9,\"1:Accept:application/json\"\r\n");
  start = HAL_GetTick();
  current = HAL_GetTick();
  while ((current - start) < timeout)
  {
    dog.reload();
    current = HAL_GetTick();
    receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
    if (inputStringLpuart1.ready == 1)
    {
      inputStringLpuart1.ready = 0;
      char * detectStr = strstr(inputStringLpuart1.string, "OK");
      if(detectStr != NULL) {
        result++;
        break;
      }
    }
  }

  if(result != 4 && reInitCount == 0) {
    DEBUG("Init http FAIL, reInit.\r\n");
    reInitCount++;
    return (funcptr)nb_initHttp;
  }

  vTaskDelay(pdMS_TO_TICKS(500));
  dog.reload();

  writeString(DEBUG_USART, "(UART) AT+UDELFILE=\"rp1\"\r\n");
  writeString(LPUART1, "AT+UDELFILE=\"rp1\"\r\n");
  start = HAL_GetTick();
  current = HAL_GetTick();
  while ((current - start) < timeout)
  {
    dog.reload();
    current = HAL_GetTick();
    receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
    if (inputStringLpuart1.ready == 1)
    {
      inputStringLpuart1.ready = 0;
      char * detectStr = strstr(inputStringLpuart1.string, "OK");
      if(detectStr != NULL) break;
    }
  }

  vTaskDelay(pdMS_TO_TICKS(500));
  dog.reload();

  return (funcptr)nb_sendData;
}

funcptr nb_sendData() {

  /* Send data */
  writeString(DEBUG_USART, "(UART) AT+UHTTPC=0,4,");
  writeString(DEBUG_USART, connectPath);
  writeString(DEBUG_USART, ",\"rp1\",\"data1\",4\r\n");

  writeString(LPUART1, "AT+UHTTPC=0,4,");
  writeString(LPUART1, connectPath);
  writeString(LPUART1, ",\"rp1\",\"data1\",4\r\n");

  char msg[1000] = {0};
  uint32_t start = HAL_GetTick();
  uint32_t current = HAL_GetTick();
  uint32_t timeout = 3000;
  while ((current - start) < timeout)
  {
    dog.reload();
    current = HAL_GetTick();
    receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
    if (inputStringLpuart1.ready == 1)
    {
      inputStringLpuart1.ready = 0;
      strcat(msg, inputStringLpuart1.string);
    }
  }

  vTaskDelay(pdMS_TO_TICKS(1000));

  char *str = strstr(msg, "OK");
  if(str != NULL) { /* Send command successful */
    DEBUG("Send is OK\r\n");
    vTaskDelay(pdMS_TO_TICKS(2000));
    return (funcptr)nb_getResponse;
  }

  DEBUG("Save in eeprom.\r\n");
  writeString(DEBUG_USART, "This data store at : ");writeUint32(DEBUG_USART, eepromFlag.nextSaveAddress);
  writeString(DEBUG_USART, "\r\n");
  parSave((uint8_t*)newData, eepromFlag.nextSaveAddress, payloadLen);
  changeSaveAddress(eepromFlag.nextSaveAddress + payloadLen);
  writeString(DEBUG_USART, "Next store at : ");writeUint32(DEBUG_USART, eepromFlag.nextSaveAddress);
  writeString(DEBUG_USART, "\r\n");

  return (funcptr)fsm_sleep;
}

funcptr nb_getResponse() 
{
  // DEBUG("nb_getResponse\r\n");

  char msg[800 + 1] = {0};
  int len = 0;
  lpuart1DataLength = 0;

  /* Check http status is 200 */
  writeString(DEBUG_USART, "(UART) AT+URDBLOCK=\"rp1\",0,40\r\n");
  writeString(LPUART1, "AT+URDBLOCK=\"rp1\",0,40\r\n");
  vTaskDelay(pdMS_TO_TICKS(100));
  len = lpuart1DataLength;
  receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
  if (inputStringLpuart1.ready == 1) {
    inputStringLpuart1.ready = 0;
    DEBUG("Part response : ");writeData(DEBUG_USART, inputStringLpuart1.string, len);
  }

  char *partMsg = strstr(inputStringLpuart1.string, "200");
  if(partMsg == NULL) {
    DEBUG("Result : FAIL!\r\n");
    writeString(DEBUG_USART, "This data store at : ");writeUint32(DEBUG_USART, eepromFlag.nextSaveAddress);
    writeString(DEBUG_USART, "\r\n");
    parSave((uint8_t*)newData, eepromFlag.nextSaveAddress, payloadLen);
    changeSaveAddress(eepromFlag.nextSaveAddress + payloadLen);
    writeString(DEBUG_USART, "Next store at : ");writeUint32(DEBUG_USART, eepromFlag.nextSaveAddress);
    writeString(DEBUG_USART, "\r\n");
    return (funcptr)fsm_sleep;
  }

  memset(msg, 0, sizeof(msg));
  writeString(DEBUG_USART, "(UART) AT+URDFILE=\"rp1\"\r\n");
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
    nbiotGetIPFlag = false;
    getOKFlag = true;
    if(amountOfData > 1) {
      uint32_t finalAddr = 0;
      for(int i = 1; i < dataPos; i++) {
        uint32_t addr = eepromFlag.nextSendAddress + (i - 1) * payloadLen;
        if(addr >= MAX_EEPROM_SIZE) addr = addr - MAX_EEPROM_SIZE + dataFirstAddress;
        parSave(nonData, addr, payloadLen);
        writeString(DEBUG_USART, "Delete data at : ");writeUint32(DEBUG_USART, addr);writeString(DEBUG_USART, "\r\n");
        finalAddr = addr + payloadLen;
      }

      changeSendAddress(finalAddr);
    }
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
      writeString(DEBUG_USART, "Change RT from ");writeUint32(DEBUG_USART, pResendTime);
      writeString(DEBUG_USART, " to ");writeUint32(DEBUG_USART, getResendTime);
      writeString(DEBUG_USART, "\r\n");
      nodeManager_changeResendTime((uint64_t)getResendTime);
      writeString(DEBUG_USART, "Now RT = ");writeUint32(DEBUG_USART, pResendTime);DEBUG("\r\n");
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
            writeString(DEBUG_USART, "Fix rtc time ok.\r\n");
          }
      }
    }
  }
  else {
    DEBUG("No success response, save in eeprom.\r\n");
    nbiotGetIPFlag = false;
    getErrorFlag = true;
    writeString(DEBUG_USART, "This data store at : ");writeUint32(DEBUG_USART, eepromFlag.nextSaveAddress);
    writeString(DEBUG_USART, "\r\n");
    parSave((uint8_t*)newData, eepromFlag.nextSaveAddress, payloadLen);
    changeSaveAddress(eepromFlag.nextSaveAddress + payloadLen);
    writeString(DEBUG_USART, "Next store at : ");writeUint32(DEBUG_USART, eepromFlag.nextSaveAddress);
    writeString(DEBUG_USART, "\r\n");
  }

  return (funcptr)fsm_sleep;
}

funcptr fsm_checkDataInEEPROM() {
  
}

funcptr fsm_sleep() {

  writeString(DEBUG_USART, ">>fsm_sleep<<\r\n");
  dog.reload();

  /* Close NBIOT */
  char msg[100] = {0};
  writeString(LPUART1, "AT+CPWROFF\r\n");

  uint32_t start = HAL_GetTick();
  uint32_t current = HAL_GetTick();
  uint32_t timeout = 1000;
  while ((current - start) < timeout)
  {
    dog.reload();
    current = HAL_GetTick();
    receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
    if (inputStringLpuart1.ready == 1)
    {
      inputStringLpuart1.ready = 0;
      strcat(msg, inputStringLpuart1.string);
    }
  }

  start = HAL_GetTick();
  current = HAL_GetTick();
  timeout = 10000;
  while ((current - start) < timeout)
  {
    dog.reload();
    current = HAL_GetTick();
    if (LL_GPIO_IsInputPinSet(pNBIOT_VINT, ioNBIOT_VINT) == 0)
    {
      DEBUG("V-INT is low, close power.\r\n");
      NBIOT_PowerOff();
      break;
    }
  }

  // writeString(DEBUG_USART, "Sleep time : ");writeUint32(DEBUG_USART, 600);writeString(DEBUG_USART, "\r\n");
  writeString(DEBUG_USART, "Sleep time : ");writeUint32(DEBUG_USART, pResendTime);writeString(DEBUG_USART, "\r\n");

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
  sprintf (tempStr, "%d.%d", tmpInt1, tmpInt2);
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
  HAL_Delay(10);

  LL_USART_TransmitData8(USART3,deviceNumber[0]);
  writeInt(DEBUG_USART, 1);
  LL_USART_TransmitData8(USART3,deviceNumber[1]);
  writeInt(DEBUG_USART, 1);
  LL_USART_TransmitData8(USART3,deviceNumber[2]);
  writeInt(DEBUG_USART, 1);
  LL_USART_TransmitData8(USART3,deviceNumber[3]);
  writeInt(DEBUG_USART, 1);
  LL_USART_TransmitData8(USART3,deviceNumber[4]);
  writeInt(DEBUG_USART, 1);
  LL_USART_TransmitData8(USART3,deviceNumber[5]);
  writeInt(DEBUG_USART, 1);
  LL_USART_TransmitData8(USART3,deviceNumber[6]);
  writeInt(DEBUG_USART, 1);
  LL_USART_TransmitData8(USART3,deviceNumber[7]);
  writeInt(DEBUG_USART, 1);
  writeInt(DEBUG_USART, 1);

  //turn to receive status
  RS485_Receive();
  HAL_Delay(500);
}

static void getRS485Data(char * deviceData, int receiveDataLen){

  parseHexInBuffer(&inputStringUsart3, &usart3Buf, receiveDataLen);
  if(inputStringUsart3.ready == 1){
    inputStringUsart3.ready = 0;
    binaryToString(inputStringUsart3.string, receiveDataLen, deviceData);
  }
}

//-----------------------------------------------------------------
