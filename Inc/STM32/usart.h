/**
  ******************************************************************************
  * File Name          : USART.h
  * Description        : This file provides code for the configuration
  *                      of the USART instances.
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __usart_H
#define __usart_H
#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */
char sendChar(USART_TypeDef * usart, char c);
int writeString(USART_TypeDef * usart, const char * s);
int  writeInt(USART_TypeDef * usart, const int n);
int writeData(USART_TypeDef * usart, const char * s, int dataLen);
int  writeUint32(USART_TypeDef * usart, const uint32_t n);
int binaryToString(const char * s, int dataLen, char * payloadOut);
void floatToString(float value, char * str, int dot);

void DEBUG(const char * s );
int DEBUG_uint(const uint32_t n);
/* USER CODE END Private defines */

void MX_LPUART1_UART_Init(void);
void MX_UART4_Init(void);
void MX_USART1_UART_Init(void);
void MX_USART2_UART_Init(void);
void MX_USART3_UART_Init(void);

void UART4_disableInterrupts(void);
void UART4_enableInterrupts(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif
#endif /*__ usart_H */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
