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

int usart1DataLength = 0;
int lpuart1DataLength = 0;
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
TIM_HandleTypeDef htim1;
I2C_HandleTypeDef hi2c4;

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
    int currentPos = DMA_LPUART1_BUFFER_SIZE - LL_DMA_GetDataLength(DMA2, LL_DMA_CHANNEL_7);
    len = currentPos - beforePosLpuart1;
    if(len < 0) len += DMA_LPUART1_BUFFER_SIZE;
    lpuart1DataLength += len;
    beforePosLpuart1 = currentPos;
    
    DEBUG("(NBIOT) \r\n");
    while(Lpuart1StartPos != currentPos) {
      ringBuffer_write(&lpuart1Buf, dmaLpuart1Buffer[Lpuart1StartPos]);
      if(len < 1000) sendChar(DEBUG_USART, dmaLpuart1Buffer[Lpuart1StartPos]);
      Lpuart1StartPos = (Lpuart1StartPos + 1) % DMA_LPUART1_BUFFER_SIZE;
    }
    DEBUG("\r\n");

    Lpuart1StartPos = currentPos;
  }
}

void USART1_IRQHandler(void)
{
  LL_USART_ClearFlag_IDLE(USART1);

  int currentPos = DMA_USART1_BUFFER_SIZE - LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_5);
  usart1DataLength = currentPos - beforePosUsart1;
  if (usart1DataLength < 0) usart1DataLength += DMA_USART1_BUFFER_SIZE;
  beforePosUsart1 = currentPos;

  while(Usart1StartPos != currentPos) {
    ringBuffer_write(&usart1Buf, dmaUsart1Buffer[Usart1StartPos]);
    Usart1StartPos = (Usart1StartPos + 1) % DMA_USART1_BUFFER_SIZE;
  }

  Usart1StartPos = currentPos;
}

/**
  * @brief This function handles USART2 global interrupt.
  */
void USART2_IRQHandler(void)
{
  LL_USART_ClearFlag_IDLE(USART2);

  int currentPos = DMA_USART2_BUFFER_SIZE - LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_6);
  enum ringBufferStatus status;

  while(Usart2StartPos != currentPos) {
     if((status = ringBuffer_write(&usart2Buf, dmaUsart2Buffer[Usart2StartPos])) == BUFFER_FULL) {
      break;
     }
     Usart2StartPos = (Usart2StartPos + 1) % DMA_USART2_BUFFER_SIZE;
  }

  Usart2StartPos = currentPos;
}

void USART3_IRQHandler(void)
{
  LL_USART_ClearFlag_IDLE(USART3);

  int currentPos = DMA_USART3_BUFFER_SIZE - LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_3);
  enum ringBufferStatus status;

  while (Usart3StartPos != currentPos)
  {
    if ((status = ringBuffer_write(&usart3Buf,dmaUsart3Buffer[Usart3StartPos])) == BUFFER_FULL)
    {
      break;
    }
    Usart3StartPos = (Usart3StartPos + 1) % DMA_USART3_BUFFER_SIZE;
  }

  Usart3StartPos = currentPos;
}

void UART4_IRQHandler(void) {
  if(LL_USART_IsActiveFlag_RXNE(UART4)) {
    char c = LL_USART_ReceiveData8(UART4);
    ringBuffer_write(&uart4Buf, c);
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
