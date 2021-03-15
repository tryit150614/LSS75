/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32l4xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "stm32l4xx_it.h"
#include "ringBuffer1.h"

int startPos = 0;
int beforePosLpuart1 = 0;
int beforePosUsart1 = 0;
unsigned int Usart1StartPos = 0;
unsigned int Usart2StartPos = 0;
unsigned int Usart3StartPos = 0;
unsigned int Lpuart1StartPos = 0;

unsigned int usart1DataLength = 0;
unsigned int lpuart1DataLength = 0;
/* External variables --------------------------------------------------------*/
extern volatile ringBufferStruct lpuart1Buf;
extern volatile ringBufferStruct usart1Buf;
extern volatile ringBufferStruct usart2Buf;
extern volatile ringBufferStruct usart3Buf;
extern volatile ringBufferStruct uart4Buf;
extern uint8_t dmaUsart1Buffer[DMA_USART1_BUFFER_SIZE];
extern uint8_t dmaUsart2Buffer[DMA_USART2_BUFFER_SIZE];
extern uint8_t dmaUsart3Buffer[DMA_USART3_BUFFER_SIZE];
extern uint8_t dmaLpuart1Buffer[DMA_LPUART1_BUFFER_SIZE];
extern TIM_HandleTypeDef htim1;
extern I2C_HandleTypeDef hi2c4;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */ 
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */

  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32L4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32l4xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles DMA1 channel1 global interrupt.
  */
void DMA1_Channel1_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel1_IRQn 0 */

  /* USER CODE END DMA1_Channel1_IRQn 0 */
  
  /* USER CODE BEGIN DMA1_Channel1_IRQn 1 */

  /* USER CODE END DMA1_Channel1_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel6 global interrupt.
  */
void DMA1_Channel6_IRQHandler(void)
{
  /* Check half-transfer complete interrupt */
  if (LL_DMA_IsEnabledIT_HT(DMA1, LL_DMA_CHANNEL_6) && LL_DMA_IsActiveFlag_HT6(DMA1)) {
    LL_DMA_ClearFlag_HT6(DMA1);             /* CLear half-transfer complete flag */

    int remainingBytesNumber = LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_6);
    int currentPos = DMA_USART2_BUFFER_SIZE - remainingBytesNumber;
    enum ringBufferStatus status;

  while(Usart2StartPos != currentPos) {
     if((status = ringBuffer_write(&usart2Buf, dmaUsart2Buffer[Usart2StartPos])) == BUFFER_FULL) {
      break;
     }
     Usart2StartPos = (Usart2StartPos + 1) % DMA_USART2_BUFFER_SIZE;
  }
    Usart2StartPos = currentPos;
  }

  /* Check transfer-complete interrupt */
  if (LL_DMA_IsEnabledIT_TC(DMA1, LL_DMA_CHANNEL_6) && LL_DMA_IsActiveFlag_TC6(DMA1)) {
    LL_DMA_ClearFlag_TC6(DMA1);             /* CLear half-transfer complete flag */

    int remainingBytesNumber = LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_6);
    int currentPos = DMA_USART2_BUFFER_SIZE - remainingBytesNumber;
    enum ringBufferStatus status;

  while(Usart2StartPos != currentPos) {
    if((status = ringBuffer_write(&usart2Buf, dmaUsart2Buffer[Usart2StartPos])) == BUFFER_FULL) {
    break;
    }
    Usart2StartPos = (Usart2StartPos + 1) % DMA_USART2_BUFFER_SIZE;
  }

    Usart2StartPos = 0;
  }
}

/**
  * @brief This function handles ADC1 global interrupt.
  */
void ADC1_IRQHandler(void)
{
  if( LL_ADC_IsActiveFlag_EOC(ADC1) )
  {
    LL_ADC_ClearFlag_EOC(ADC1);
  }
  if( LL_ADC_IsActiveFlag_EOS(ADC1) )
  {
    LL_ADC_ClearFlag_EOS(ADC1);
  }
}

/**
  * @brief This function handles TIM1 update interrupt and TIM16 global interrupt.
  */
void TIM1_UP_TIM16_IRQHandler(void)
{
  /* USER CODE BEGIN TIM1_UP_TIM16_IRQn 0 */

  /* USER CODE END TIM1_UP_TIM16_IRQn 0 */
  HAL_TIM_IRQHandler(&htim1);
  /* USER CODE BEGIN TIM1_UP_TIM16_IRQn 1 */

  /* USER CODE END TIM1_UP_TIM16_IRQn 1 */
}

void I2C4_EV_IRQHandler(void)
{
  /* USER CODE BEGIN I2C4_EV_IRQn 0 */

  /* USER CODE END I2C4_EV_IRQn 0 */
  HAL_I2C_EV_IRQHandler(&hi2c4);
  /* USER CODE BEGIN I2C4_EV_IRQn 1 */

  /* USER CODE END I2C4_EV_IRQn 1 */
}

/**
  * @brief This function handles I2C4 error interrupt.
  */
void I2C4_ER_IRQHandler(void)
{
  /* USER CODE BEGIN I2C4_ER_IRQn 0 */

  /* USER CODE END I2C4_ER_IRQn 0 */
  HAL_I2C_ER_IRQHandler(&hi2c4);
  /* USER CODE BEGIN I2C4_ER_IRQn 1 */

  /* USER CODE END I2C4_ER_IRQn 1 */
}

void LPUART1_IRQHandler(void)
{
  if(LL_LPUART_IsActiveFlag_IDLE(LPUART1)) {
    LL_LPUART_ClearFlag_IDLE(LPUART1);

    int len = 0;
    int remainingBytesNumber = LL_DMA_GetDataLength(DMA2, LL_DMA_CHANNEL_7);
    int currentPos = DMA_LPUART1_BUFFER_SIZE - remainingBytesNumber;
    len = currentPos - beforePosLpuart1;
    if (len < 0)
      len += DMA_LPUART1_BUFFER_SIZE;

    lpuart1DataLength += len;
    beforePosLpuart1 = currentPos;
    
    // DEBUG("this len = ");writeUint32(DEBUG_USART , len);DEBUG("\r\n");
    // DEBUG("lpuart1DataLength = ");writeUint32(DEBUG_USART , lpuart1DataLength);DEBUG("\r\n");
    // DEBUG("Lpuart1StartPos = ");writeUint32(DEBUG_USART , Lpuart1StartPos);DEBUG("\r\n");
    // DEBUG("currentPos = ");writeUint32(DEBUG_USART , currentPos);DEBUG("\r\n");
    writeString(DEBUG_USART, "(NBIOT) \r\n");
    // enum ringBufferStatus status;
    while(Lpuart1StartPos != currentPos) {
       ringBuffer_write(&lpuart1Buf, dmaLpuart1Buffer[Lpuart1StartPos]);
      //  sendChar(DEBUG_USART, dmaLpuart1Buffer[Lpuart1StartPos]);
       sendChar(DEBUG_USART, lpuart1Buf.bufferPtr[lpuart1Buf.writeIndex - 1]);
       Lpuart1StartPos = (Lpuart1StartPos + 1) % DMA_LPUART1_BUFFER_SIZE;
    }
    writeString(DEBUG_USART, "\r\n");
    // DEBUG("writeIndex = ");writeUint32(DEBUG_USART , lpuart1Buf.writeIndex);DEBUG("\r\n\r\n");

    Lpuart1StartPos = currentPos;
  }
}

void USART1_IRQHandler(void)
{
  LL_USART_ClearFlag_IDLE(USART1);

  int remainingBytesNumber = LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_5);
  int currentPos = DMA_USART1_BUFFER_SIZE - remainingBytesNumber;
  usart1DataLength = currentPos - beforePosUsart1;
  if (usart1DataLength < 0)
    usart1DataLength += DMA_USART1_BUFFER_SIZE;
  beforePosUsart1 = currentPos;
  // enum ringBufferStatus status;

  // writeString(DEBUG_USART, "=================USART1==================\r\n");
  while(Usart1StartPos != currentPos) {
    //  if((status = ringBuffer_write(&usart1Buf, dmaUsart1Buffer[Usart1StartPos])) == BUFFER_FULL) {
    //   break;
    //  }

    // sendChar(DEBUG_USART, dmaUsart1Buffer[Usart1StartPos]);
    ringBuffer_write(&usart1Buf, dmaUsart1Buffer[Usart1StartPos]);
    Usart1StartPos = (Usart1StartPos + 1) % DMA_USART1_BUFFER_SIZE;
  }

  // writeString(DEBUG_USART, "\r\n===================================\r\n");
  // ringBuffer_write(&usart1Buf, '\n');
  Usart1StartPos = currentPos;
}

/**
  * @brief This function handles USART2 global interrupt.
  */
void USART2_IRQHandler(void)
{
  LL_USART_ClearFlag_IDLE(USART2);

  int remainingBytesNumber = LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_6);
  int currentPos = DMA_USART2_BUFFER_SIZE - remainingBytesNumber;
  enum ringBufferStatus status;

  // writeString(DEBUG_USART, "=================USART2==================\r\n");
  while(Usart2StartPos != currentPos) {
     if((status = ringBuffer_write(&usart2Buf, dmaUsart2Buffer[Usart2StartPos])) == BUFFER_FULL) {
      break;
     }
    //  sendChar(DEBUG_USART, dmaUsart2Buffer[Usart2StartPos]);
     Usart2StartPos = (Usart2StartPos + 1) % DMA_USART2_BUFFER_SIZE;
  }
  // writeString(DEBUG_USART, "\r\n===================================\r\n");

  Usart2StartPos = currentPos;
}

void USART3_IRQHandler(void)
{
  LL_USART_ClearFlag_IDLE(USART3);

  int remainingBytesNumber = LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_3);
  int currentPos = DMA_USART3_BUFFER_SIZE - remainingBytesNumber;
  enum ringBufferStatus status;

  // writeString(DEBUG_USART, "=============DMA===================\r\n ");
  while (Usart3StartPos != currentPos)
  {
    if ((status = ringBuffer_write(&usart3Buf,dmaUsart3Buffer[Usart3StartPos])) == BUFFER_FULL)
    {
      break;
    }
    // sendChar(DEBUG_USART, dmaUsart3Buffer[Usart3StartPos]);
    Usart3StartPos = (Usart3StartPos + 1) % DMA_USART3_BUFFER_SIZE;
  }
  // writeString(DEBUG_USART, "\r\n================================\r\n ");

  Usart3StartPos = currentPos;
}

void UART4_IRQHandler(void) {
  if(LL_USART_IsActiveFlag_RXNE(UART4)) {
    char c = LL_USART_ReceiveData8(UART4);
    ringBuffer_write(&uart4Buf, c);
  }

  if(LL_USART_IsActiveFlag_CM(UART4)) {
    LL_USART_ClearFlag_CM(UART4);
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
