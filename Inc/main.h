/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"
#include "stm32l4xx_hal_flash.h"
#include "stm32l4xx_ll_adc.h"
#include "stm32l4xx_ll_dma.h"
#include "stm32l4xx_ll_lpuart.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_crs.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_system.h"
#include "stm32l4xx_ll_exti.h"
#include "stm32l4xx_ll_cortex.h"
#include "stm32l4xx_ll_utils.h"
#include "stm32l4xx_ll_pwr.h"
#include "stm32l4xx_ll_usart.h"
#include "stm32l4xx.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_rtc.h"
#include "stm32l4xx_hal_rng.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define DMA_LPUART1_BUFFER_SIZE 12000
#define DMA_USART1_BUFFER_SIZE  12000
#define DMA_USART2_BUFFER_SIZE  1000
#define DMA_USART3_BUFFER_SIZE  100

#define DMA1_ADC_BUFFER_SIZE    4

#define LORA_USART    USART2
#define DEBUG_USART   UART4

#define pRS485Switch        GPIOA
#define pPowerLED           GPIOA
#define pReceiveLED         GPIOB
#define pSendLED            GPIOB
#define pLoraPower          GPIOC
#define pRS485ICPower       GPIOA
#define pRS485SensorPower   GPIOB
#define pEEPROMProtect      GPIOB
#define pNBIOTPower         GPIOB
#define pNBIOT_PWR          GPIOB
#define pNBIOT_RESET        GPIOB
#define pNBIOT_VINT         GPIOB

#define ioRS485Switch       LL_GPIO_PIN_11
#define ioPowerLED          LL_GPIO_PIN_12
#define ioReceiveLED        LL_GPIO_PIN_12
#define ioSendLED           LL_GPIO_PIN_2
#define ioLoraPower         LL_GPIO_PIN_7
#define ioRS485ICPower      LL_GPIO_PIN_8
#define ioRS485SensorPower  LL_GPIO_PIN_9
#define ioEEPROMProtect     LL_GPIO_PIN_3
#define ioNBIOTPower        LL_GPIO_PIN_8
#define ioNBIOT_PWR         LL_GPIO_PIN_13
#define ioNBIOT_RESET       LL_GPIO_PIN_14
#define ioNBIOT_VINT        LL_GPIO_PIN_15

#define powerOnLora()             LL_GPIO_SetOutputPin(pLoraPower, ioLoraPower)
#define powerOffLora()            LL_GPIO_ResetOutputPin(pLoraPower, ioLoraPower)
#define powerOnRS485IC()          LL_GPIO_SetOutputPin(pRS485ICPower, ioRS485ICPower)
#define powerOffRS485IC()         LL_GPIO_ResetOutputPin(pRS485ICPower, ioRS485ICPower)
#define powerOnRS485Sensor()      LL_GPIO_SetOutputPin(pRS485SensorPower, ioRS485SensorPower)
#define powerOffRS485Sensor()     LL_GPIO_ResetOutputPin(pRS485SensorPower, ioRS485SensorPower)
#define Lora_Send_LED_ON()        LL_GPIO_SetOutputPin(pSendLED, ioSendLED)
#define Lora_Send_LED_OFF()       LL_GPIO_ResetOutputPin(pSendLED, ioSendLED)
#define Lora_Receive_LED_ON()     LL_GPIO_SetOutputPin(pReceiveLED, ioReceiveLED)
#define Lora_Receive_LED_OFF()    LL_GPIO_ResetOutputPin(pReceiveLED, ioReceiveLED)
#define Lora_Send_Toggle()        LL_GPIO_TogglePin(pSendLED, ioSendLED)
#define Lora_Receive_Toggle()     LL_GPIO_TogglePin(pReceiveLED, ioReceiveLED)
#define powerLED_Toggle()         LL_GPIO_TogglePin(pPowerLED, ioPowerLED)
#define RS485_Send()              LL_GPIO_SetOutputPin(pRS485Switch, ioRS485Switch)
#define RS485_Receive()           LL_GPIO_ResetOutputPin(pRS485Switch, ioRS485Switch)
#define EEPROM_EnableProtect()    LL_GPIO_SetOutputPin(pEEPROMProtect, ioEEPROMProtect)
#define EEPROM_DisableProtect()   LL_GPIO_ResetOutputPin(pEEPROMProtect, ioEEPROMProtect)
#define NBIOT_PowerOn()           LL_GPIO_SetOutputPin(pNBIOTPower, ioNBIOTPower)
#define NBIOT_PowerOff()          LL_GPIO_ResetOutputPin(pNBIOTPower, ioNBIOTPower)
/* USER CODE BEGIN Private defines */
struct FlashDataStruct {
  uint64_t resendTime;
};

struct FlashMACStruct {
  uint64_t mac;
  uint64_t hash1;
  uint64_t hash2;
};

struct FlashFlagStruct {
  uint64_t initFlag;
};

uint32_t getUnixTimestamp();
#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
