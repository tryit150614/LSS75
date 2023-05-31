#include "main.h"
#include "st_watchdog.h"
#include "cmsis_os.h"
#include "usart.h"
#include "st_rtc.h"
#include "ringBuffer1.h"
#include "serialHandlers.h"
#include "nodeManager.h"
#include "flashWrapper.h"
#include "adc.h"
#include "fsm.h"
#include "eeprom.h"
#include "Cli.h"
#include "gpio.h"
#include "dma.h"

extern struct FSMAttributes fsmAttributes;
extern Watchdog watchdog;
extern Rtc      rtc;
extern EEPROMFlag eepromFlag;

/* Read from flash */
volatile uint32_t pResendTime = 0;
volatile uint32_t pMAC = 0;
char pHash[9] = {0};
char pHttpAddr[100] = "http://yummy.agriweather.online/api/6th-nbiot";

bool reset_Flag = false;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

void TaskSystem(void * parameter);
void TaskLED(void *pvParameters);

int main(void)
{
  watchdog.enable();
  HAL_Init();
  __enable_irq();

  SystemClock_Config();
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_LPUART1_UART_Init();
  MX_USART1_UART_Init();
  MX_UART4_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_I2C4_Init();

  rtc.init();
  ringBuffer_init(); /* 初始化各USART的Buffer */
  MX_ADC1_Init();
  fsm_init();

  /* Init scheduler */
  BaseType_t xReturned;
  xReturned = xTaskCreate(TaskSystem, "taskSystem", 128, NULL, 1, NULL);
  if(xReturned != pdPASS) {
    writeString(DEBUG_USART, "Could not create task taskSystem.");
    while (1);
  }

  xReturned = xTaskCreate(TaskADC, "taskADC", 128 * 3, NULL, 1, NULL);
  if(xReturned != pdPASS) {
    writeString(DEBUG_USART, "Could not create task taskFSM.");
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

  if(flashRead(FLASH_FLAG_START_ADDR) != 0)
  {
    DEBUG("----------- init -----------\r\n");
    LED_ALL_Toggle();
    clearAllDataInEEPROM();
    LED_ALL_Toggle();
    nodeManager_initFlashData();
  }

  findFlashPosition();
  if(!eeprom_load_eepromFlag()) DEBUG("Load eepromFlag fail!\r\n");

  /* Setting rtc alert */
  RTC_SetAlert(pResendTime > 86400 ? 1200 : pResendTime);
  /***********************/

  DEBUG("Init : ");writeUint32(DEBUG_USART, flashRead(FLASH_FLAG_START_ADDR));DEBUG("\r\n");
  DEBUG("MAC : ");writeUint32(DEBUG_USART, pMAC);DEBUG("\r\n");
  DEBUG("Hash : ");DEBUG(pHash);DEBUG("\r\n");
  DEBUG("ResendTime : ");writeUint32(DEBUG_USART, pResendTime);DEBUG("\r\n");
  DEBUG("Save address : ");writeUint32(DEBUG_USART, eepromFlag.nextSaveAddress);DEBUG("\r\n");
  DEBUG("Send address : ");writeUint32(DEBUG_USART, eepromFlag.nextSendAddress);DEBUG("\r\n");

  DEBUG("FD Version 1.4.8 2023/05/31\r\n");
  DEBUG("-----------------------------------\r\n");

  /* Start scheduler */
  vTaskStartScheduler();
 
  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  while (1)
  {
    
  }
}

void TaskSystem(void * parameter) {
  for(;;) {
    watchdog.reload();
    vTaskDelay(pdMS_TO_TICKS(995));
  }
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
