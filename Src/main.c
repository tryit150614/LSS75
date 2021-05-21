/*-------------------------------------
version : 1.0.0
date    : 2020/12/07
RD      : Sean

SD card version
--------------------------------------*/
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "main.h"
#include "st_watchdog.h"
#include "cmsis_os.h"
#include "usart.h"
#include "st_rtc.h"
#include "ringBuffer1.h"
#include "serialHandlers.h"
#include "lora_rylr890.h"
#include "nodeManager.h"
#include "flashWrapper.h"
#include "adc.h"
#include "fsm.h"
#include "fsm_nbiot.h"
#include "rng.h"
#include "eeprom.h"
#include "rtcController.h"

struct FlashDataStruct flashData = {(uint64_t)1200};
struct FlashMACStruct flashMAC = {13, 0, 0};
struct FlashFlagStruct flashFlag = {1};

extern uint16_t adc1DmaBuffer[DMA1_ADC_BUFFER_SIZE];
extern struct FSMAttributes fsmAttributes;
extern struct FSMAttributes_NB fsmAttributes_NB;

extern volatile uint32_t flashDataAddr;
extern volatile uint32_t flashMACAddr;
extern const unsigned long FlashMACStructSize;

const unsigned int lpuart1DataSize = 800;
uint8_t lpuart1Data[lpuart1DataSize];
volatile ringBufferStruct lpuart1Buf;
extern struct inputString inputStringLpuart1;
extern unsigned int lpuart1DataLength;

const unsigned int usart1DataSize = 800;
uint8_t usart1Data[usart1DataSize];
volatile ringBufferStruct usart1Buf;
extern struct inputString inputStringUsart1;
extern unsigned int usart1DataLength;

const unsigned int uart4DataSize = 240;
uint8_t uart4Data[uart4DataSize];
volatile ringBufferStruct uart4Buf;
extern struct inputString inputStringUart4;

const unsigned int usart2DataSize = 200;
uint8_t usart2Data[usart2DataSize];
volatile ringBufferStruct usart2Buf;
extern struct inputString inputStringUsart2;

const unsigned int usart3DataSize = 200;
uint8_t usart3Data[usart3DataSize];
volatile ringBufferStruct usart3Buf;

volatile uint32_t pResendTime = 0;
volatile uint32_t pMAC = 0;
char pHash[9] = {0};

/* LED flag */
volatile bool rs485Flag    = false;
volatile bool getDataFlag  = false;
volatile bool nbiotFindIPFlag   = false;
volatile bool nbiotGetIPFlag    = false;
volatile bool getOKFlag     = false;
volatile bool getErrorFlag  = false;
/*----------*/

I2C_HandleTypeDef hi2c4;
EEPROMFlag eepromFlag;

/* Private variables ---------------------------------------------------------*/
extern Watchdog watchdog;
extern Rtc      rtc;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);

void MX_DMA_Init(void);
void MX_GPIO_Init(void);
void TaskSystem(void * parameter);
void TaskFSM(void *pvParameters);
void TaskCLI(void *pvParameters);
void TaskLED(void *pvParameters);
void vSendCmd(void *pvParameters);
void vGetNBIotData(void *pvParameters);
void vNBIOT_fsm(void *pvParameters);

uint32_t month2day(uint32_t month);
void ledBlinking(GPIO_TypeDef *GPIOx, uint32_t pinMask, uint32_t ligth, uint32_t dark);

int main(void)
{
  watchdog.enable();
  HAL_Init();
  __enable_irq();


  if(flashRead(FLASH_FLAG_START_ADDR) != 0)
    nodeManager_initFlashData(1, 1);

  findFlashPosition();

  SystemClock_Config();
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_LPUART1_UART_Init();
  MX_USART1_UART_Init();
  MX_UART4_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_RNG_Init();
  MX_I2C4_Init();

  rtc.init();

  /* Setting rtc alert */
  if(pResendTime == 0 || pResendTime > 86400)
    pResendTime = 1200;

  LL_PWR_EnableBkUpAccess();
  LL_RTC_DisableWriteProtection(RTC);
  LL_RTC_ClearFlag_WUT(RTC);
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
  RTC->CR &= ~RTC_CR_WUTE;
  HAL_Delay(1);
  RTC->WUTR = pResendTime;
  RTC->CR = RTC_CR_WUTE | RTC_CR_WUTIE | RTC_CR_WUCKSEL_2;
  LL_RTC_EnableWriteProtection(RTC);
  EXTI->RTSR1 |= EXTI_RTSR1_RT20;
  EXTI->IMR1 |= EXTI_IMR1_IM20;
  NVIC_EnableIRQ(RTC_WKUP_IRQn);
  NVIC_SetPriority(RTC_WKUP_IRQn, 2);
  LL_PWR_DisableBkUpAccess();
  /***********************/

  if(flashRead(FLASH_FLAG_START_ADDR) != 0)
  {
    writeString(DEBUG_USART, "----------- init -----------\r\n");
    Lora_Send_Toggle();
    Lora_Receive_Toggle();
    powerLED_Toggle();

    clearAllDataInEEPROM();
    Lora_Send_Toggle();
    Lora_Receive_Toggle();
    powerLED_Toggle();
    changeSaveAddress(dataFirstAddress);
    changeSendAddress(dataFirstAddress);
    HAL_Delay(10);
    nodeManager_riseInitFlag();
  }
  uint8_t temp[8+1] = {0};
  bool loadResult = parLoad(temp, (uint16_t)0, (uint16_t)8);
  if(loadResult) {
    eepromFlag.nextSaveAddress = (uint16_t)((temp[0] + temp[1] + temp[2] + temp[3]) * payloadLen);
    eepromFlag.nextSendAddress = (uint16_t)((temp[4] + temp[5] + temp[6] + temp[7]) * payloadLen);
    writeString(DEBUG_USART, "Save address : ");writeUint32(DEBUG_USART, eepromFlag.nextSaveAddress);writeString(DEBUG_USART, "\r\n");
    writeString(DEBUG_USART, "Send address : ");writeUint32(DEBUG_USART, eepromFlag.nextSendAddress);writeString(DEBUG_USART, "\r\n");
  }
  else {
    writeString(DEBUG_USART, "Load saveADdress & sendAddress FAIL!\r\n");
  }

  writeString(DEBUG_USART, "Init : ");writeUint32(DEBUG_USART, flashRead(FLASH_FLAG_START_ADDR));writeString(DEBUG_USART, "\r\n");
  writeString(DEBUG_USART, "MAC : ");writeUint32(DEBUG_USART, pMAC);writeString(DEBUG_USART, "\r\n");
  writeString(DEBUG_USART, "Hash : ");writeString(DEBUG_USART, pHash);writeString(DEBUG_USART, "\r\n");
  writeString(DEBUG_USART, "pResendTime : ");writeUint32(DEBUG_USART, pResendTime);writeString(DEBUG_USART, "\r\n");
  writeString(DEBUG_USART, "-----------------------------------\r\n");
  DEBUG("Version 1.2.0 2021/05/20\r\n");

  ringBuffer_new(&lpuart1Buf, lpuart1Data, lpuart1DataSize);
  ringBuffer_resetBuffer(&lpuart1Buf);

  ringBuffer_new(&usart1Buf, usart1Data, usart1DataSize);
  ringBuffer_resetBuffer(&usart1Buf);

  ringBuffer_new(&uart4Buf, uart4Data, uart4DataSize);
  ringBuffer_resetBuffer(&uart4Buf);

  ringBuffer_new(&usart2Buf, usart2Data, usart2DataSize);
  ringBuffer_resetBuffer(&usart2Buf);

  ringBuffer_new(&usart3Buf, usart3Data, usart3DataSize);
  ringBuffer_resetBuffer(&usart3Buf);

  powerOnRS485IC();
  // powerOnLora();
  EEPROM_EnableProtect();
  NBIOT_PowerOn();

  MX_ADC1_Init();
  fsm_init();

  /* Init scheduler */
  BaseType_t xReturned;
  xReturned = xTaskCreate(TaskSystem, "taskSystem", 128, NULL, 1, NULL);
  if(xReturned != pdPASS) {
    writeString(DEBUG_USART, "Could not create task taskSystem.");
    while (1);
  }

  xReturned = xTaskCreate(TaskFSM, "taskFSM", 128 * 25, NULL, 1, NULL);
  if(xReturned != pdPASS) {
    writeString(DEBUG_USART, "Could not create task taskFSM.");
    while (1);
  }

  xReturned = xTaskCreate(TaskCLI, "taskCLI", 128, NULL, 1, NULL);
  if(xReturned != pdPASS) {
    writeString(DEBUG_USART, "Could not create task CLI.");
    while (1);
  }

  xReturned = xTaskCreate(TaskLED, "taskLED", 200, NULL, 1, NULL);
  if(xReturned != pdPASS) {
    writeString(DEBUG_USART, "Could not create task LED.");
    while (1);
  }

  DEBUG("READY\r\n");

  /* Start scheduler */
  vTaskStartScheduler();
 
  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  while (1)
  {
    
  }
}

void MX_DMA_Init(void) 
{
  /* DMA controller clock enable */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);
}

void MX_GPIO_Init(void)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOH);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);

  /* RS485 switch (origin is PB12) */
  GPIO_InitStruct.Pin = ioRS485Switch;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(pRS485Switch, &GPIO_InitStruct);

  /* Power LED */
  GPIO_InitStruct.Pin = ioPowerLED;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(pPowerLED, &GPIO_InitStruct);

  /* Receive LED */
  GPIO_InitStruct.Pin = ioReceiveLED;
  LL_GPIO_Init(pReceiveLED, &GPIO_InitStruct);

  /* Send LED */
  GPIO_InitStruct.Pin = ioSendLED;
  LL_GPIO_Init(pSendLED, &GPIO_InitStruct);

  /* Lora power */
  GPIO_InitStruct.Pin = ioLoraPower;
  LL_GPIO_Init(pLoraPower, &GPIO_InitStruct);

  /* RS485 IC power */
  GPIO_InitStruct.Pin = ioRS485ICPower;
  LL_GPIO_Init(pRS485ICPower, &GPIO_InitStruct);

  /* RS485 sensor power */
  GPIO_InitStruct.Pin = ioRS485SensorPower;
  LL_GPIO_Init(pRS485SensorPower, &GPIO_InitStruct);

  /* NBIOT power */
  GPIO_InitStruct.Pin = ioNBIOTPower;
  LL_GPIO_Init(pNBIOTPower, &GPIO_InitStruct);

  /* NBIOT PWR-ON */
  GPIO_InitStruct.Pin = ioNBIOT_PWR;
  LL_GPIO_Init(pNBIOT_PWR, &GPIO_InitStruct);
  LL_GPIO_SetOutputPin(pNBIOT_PWR, ioNBIOT_PWR);

  /* NBIOT RESET-N */
  // GPIO_InitStruct.Pin = ioNBIOT_RESET;
  // LL_GPIO_Init(pNBIOT_RESET, &GPIO_InitStruct);
  // LL_GPIO_SetOutputPin(pNBIOT_RESET, ioNBIOT_RESET);

  /* I2C protect pin */
  GPIO_InitStruct.Pin = ioEEPROMProtect;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(pEEPROMProtect, &GPIO_InitStruct);

  /* NBIOT V-INT */
  GPIO_InitStruct.Pin = ioNBIOT_VINT;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(pNBIOT_VINT, &GPIO_InitStruct);
}

void TaskSystem(void * parameter) {
  for(;;) {
    watchdog.reload();
    LL_GPIO_TogglePin(pPowerLED, ioPowerLED);
    vTaskDelay(pdMS_TO_TICKS(5));
    LL_GPIO_TogglePin(pPowerLED, ioPowerLED);
    vTaskDelay(pdMS_TO_TICKS(995));
  }
}

void TaskFSM(void *pvParameters) {
  for (;;) {
      fsmAttributes.state = (StateFunc)(*fsmAttributes.state)();
  }
}

void TaskCLI(void *pvParameters) {
  for (;;) {
      parseStringInBuffer(&inputStringUart4, &uart4Buf);
      if (inputStringUart4.ready == 1) {
          inputStringUart4.ready = 0;
          /* $00011FWzw5ad6 */
          if(inputStringUart4.string[0] == '$') {
              char macStr[6] = {0};
              memcpy(macStr, inputStringUart4.string + 1, 5);
              memcpy(pHash, inputStringUart4.string + 6, 8);
              uint32_t mac = strtol(macStr, NULL, 10);
              uint32_t hash1 = (uint32_t)((uint8_t)(pHash[0]) << 24 |
                                          (uint8_t)(pHash[1]) << 16 |
                                          (uint8_t)(pHash[2]) << 8 |
                                          (uint8_t)(pHash[3]));
              uint32_t hash2 = (uint32_t)((uint8_t)(pHash[4]) << 24 |
                                          (uint8_t)(pHash[5]) << 16 |
                                          (uint8_t)(pHash[6]) << 8 |
                                          (uint8_t)(pHash[7]));
              writeString(DEBUG_USART, "******* Change MAC to ");
              writeUint32(DEBUG_USART, mac);
              writeString(DEBUG_USART, " *******\r\n");
              writeString(DEBUG_USART, "******* Change hash to ");
              writeString(DEBUG_USART, pHash);
              writeString(DEBUG_USART, " *******\r\n");

              nodeManager_modifyMACandHash(mac, hash1, hash2);

              // rs485Flag = false;
          }
      }

      vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void TaskLED(void *pvParameters) {
  for(;;) {
    if(rs485Flag) {
      LL_GPIO_SetOutputPin( pPowerLED, ioPowerLED);
      LL_GPIO_SetOutputPin( pSendLED, ioSendLED);
      LL_GPIO_SetOutputPin( pReceiveLED, ioReceiveLED);
      vTaskDelay(pdMS_TO_TICKS(5));
      LL_GPIO_ResetOutputPin( pPowerLED, ioPowerLED);
      LL_GPIO_ResetOutputPin( pSendLED, ioSendLED);
      LL_GPIO_ResetOutputPin( pReceiveLED, ioReceiveLED);
      vTaskDelay(pdMS_TO_TICKS(995));
    }
    else if(getDataFlag) {
      LL_GPIO_SetOutputPin( pPowerLED, ioPowerLED);
      vTaskDelay(pdMS_TO_TICKS(5));
      LL_GPIO_ResetOutputPin( pPowerLED, ioPowerLED);
      vTaskDelay(pdMS_TO_TICKS(995));
    }
    else if(nbiotFindIPFlag) {
      LL_GPIO_SetOutputPin( pReceiveLED, ioReceiveLED);
      vTaskDelay(pdMS_TO_TICKS(10));
      LL_GPIO_ResetOutputPin( pReceiveLED, ioReceiveLED);
      vTaskDelay(pdMS_TO_TICKS(990));
    }
    else if(nbiotGetIPFlag) {
      LL_GPIO_SetOutputPin( pSendLED, ioSendLED);
      vTaskDelay(pdMS_TO_TICKS(10));
      LL_GPIO_ResetOutputPin( pSendLED, ioSendLED);
      vTaskDelay(pdMS_TO_TICKS(990));
    }
    else if(getOKFlag) {
      LL_GPIO_SetOutputPin( pPowerLED, ioPowerLED);
      LL_GPIO_SetOutputPin( pSendLED, ioSendLED);
      LL_GPIO_SetOutputPin( pReceiveLED, ioReceiveLED);
      vTaskDelay(pdMS_TO_TICKS(100));
      LL_GPIO_ResetOutputPin( pPowerLED, ioPowerLED);
      LL_GPIO_ResetOutputPin( pReceiveLED, ioReceiveLED);
      LL_GPIO_ResetOutputPin( pSendLED, ioSendLED);
      vTaskDelay(pdMS_TO_TICKS(900));
    }
    else if(getErrorFlag) {
      LL_GPIO_SetOutputPin( pReceiveLED, ioReceiveLED);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void vSendCmd(void *pvParameters) {
  for(;;) {
    vTaskDelay(pdMS_TO_TICKS(1));
    // sendBinaryInBuffer(&inputStringUsart1, &usart1Buf);
    parseStringInBuffer(&inputStringUsart2, &usart2Buf);
    if (inputStringUsart2.ready == 1) {
      inputStringUsart2.ready = 0;
      // writeString(DEBUG_USART, "LEN = ");writeInt(DEBUG_USART, usart1DataLength);
      // writeString(DEBUG_USART, ", Send : ");
      // writeString(DEBUG_USART, inputStringUsart2.string);writeString(DEBUG_USART, "\r\n");
      writeString(LPUART1, inputStringUsart2.string);
    }
  }
}

void vGetNBIotData(void *pvParameters) {
  int len = 0;
  for(;;) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    len = lpuart1DataLength;
    receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
    if (inputStringLpuart1.ready == 1) {
      inputStringLpuart1.ready = 0;
      // writeString(DEBUG_USART, "NBIOT Response, LEN = ");writeInt(DEBUG_USART, len);writeString(DEBUG_USART, "\r\n");
      writeString(DEBUG_USART, "-----------------Task------------------\r\n");
      writeData(DEBUG_USART, inputStringLpuart1.string, len);
      writeString(DEBUG_USART, "\r\n-----------------------------------\r\n");
      // writeData(DEBUG_USART, (char *)lpuart1Buf.bufferPtr, 800);
    }
  }
}

void vNBIOT_fsm(void *pvParameters) {
  for (;;) {
      fsmAttributes_NB.state = (StateNB)(*fsmAttributes_NB.state)();
  }
}

uint32_t month2day(uint32_t month) {
  switch(month - 1) {
    case 0: return 0;
    break;
    case 1: return 31;
    break;
    case 2: return 31+28;
    break;
    case 3: return 31+28+31;
    break;
    case 4: return 31+28+31+30;
    break;
    case 5: return 31+28+31+30+31;
    break;
    case 6: return 31+28+31+30+31+30;
    break;
    case 7: return 31+28+31+30+31+30+31;
    break;
    case 8: return 31+28+31+30+31+30+31+31;
    break;
    case 9: return 31+28+31+30+31+30+31+31+30;
    break;
    case 10: return 31+28+31+30+31+30+31+31+30+31;
    break;
    case 11: return 31+28+31+30+31+30+31+31+30+31+30;
    break;
  }

  return 0;
}

uint32_t getUnixTimestamp() {

  uint8_t leapYearCount = 0;
  uint32_t baseSeconds = 946684800; // It's 1970/1/1~2020/1/1 total seconds

  volatile uint32_t second = rtc.getSecond();
  volatile uint32_t minute = rtc.getMinute();
  volatile uint32_t hour = rtc.getHour();
  volatile uint32_t day = rtc.getDay();
  volatile uint32_t month = rtc.getMonth();
  volatile uint32_t year = rtc.getYear();

  leapYearCount = year / 4 + 1; //unit : day
  if(year % 4 == 0 && month < 3) leapYearCount--;

  return  baseSeconds + (year * 365 * 86400) +
          (month2day(month) + day + leapYearCount - 1) * 86400 +
          (hour * 3600) + (minute * 60) + second;
}

void ledBlinking(GPIO_TypeDef *GPIOx, uint32_t pinMask, uint32_t ligth, uint32_t dark) {
  LL_GPIO_SetOutputPin(GPIOx, pinMask);
  HAL_Delay(ligth);
  LL_GPIO_ResetOutputPin(GPIOx, pinMask);
  HAL_Delay(dark);
}

void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);

  if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_0)
  {
  Error_Handler();  
  }
  LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
  LL_RCC_HSE_Enable();

   /* Wait till HSE is ready */
  while(LL_RCC_HSE_IsReady() != 1)
  {
    
  }
  LL_PWR_EnableBkUpAccess();
  if(flashRead(FLASH_FLAG_START_ADDR) != 0) {
    LL_RCC_ForceBackupDomainReset();
  }
  LL_RCC_ReleaseBackupDomainReset();
  LL_RCC_LSE_SetDriveCapability(LL_RCC_LSEDRIVE_HIGH);
  LL_RCC_LSE_Enable();

   /* Wait till LSE is ready */
  while(LL_RCC_LSE_IsReady() != 1)
  {
    
  }
  LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSE);
  LL_RCC_EnableRTC();
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_1, 8, LL_RCC_PLLR_DIV_8);
  LL_RCC_PLL_EnableDomain_SYS();
  LL_RCC_PLL_Enable();

   /* Wait till PLL is ready */
  while(LL_RCC_PLL_IsReady() != 1)
  {
    
  }
  LL_RCC_PLLSAI1_ConfigDomain_ADC(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_1, 8, LL_RCC_PLLSAI1R_DIV_8);
  LL_RCC_PLLSAI1_EnableDomain_ADC();
  LL_RCC_PLLSAI1_Enable();

   /* Wait till PLLSAI1 is ready */
  while(LL_RCC_PLLSAI1_IsReady() != 1)
  {
    
  }
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
  {
  
  }
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);

   /* Update the time base */
  if (HAL_InitTick (TICK_INT_PRIORITY) != HAL_OK)
  {
    Error_Handler();  
  };
  LL_Init1msTick(16000000);
  LL_SYSTICK_SetClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
  LL_SetSystemCoreClock(16000000);
  LL_RCC_SetUSARTClockSource(LL_RCC_USART1_CLKSOURCE_PCLK2);
  LL_RCC_SetUSARTClockSource(LL_RCC_USART2_CLKSOURCE_PCLK1);
  LL_RCC_SetUSARTClockSource(LL_RCC_USART3_CLKSOURCE_PCLK1);
  LL_RCC_SetUARTClockSource(LL_RCC_UART4_CLKSOURCE_PCLK1);
  LL_RCC_SetLPUARTClockSource(LL_RCC_LPUART1_CLKSOURCE_PCLK1);
  LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSOURCE_PLLSAI1);
  LL_RCC_SetRNGClockSource(LL_RCC_RNG_CLKSOURCE);
  LL_RCC_SetI2CClockSource(LL_RCC_I2C4_CLKSOURCE_PCLK1);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
}

void Error_Handler(void)
{
  writeString(DEBUG_USART, "Error : Enter error_handler.\r\n");

  while(1) {
    LL_GPIO_TogglePin(pReceiveLED, ioReceiveLED);
    LL_GPIO_TogglePin(pSendLED,    ioSendLED);
    HAL_Delay(1000);
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
