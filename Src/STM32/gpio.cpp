#include "gpio.h"
#include "main.h"
#include "adc.h"
#include "cmsis_os.h"

extern bool adc_Flag; // From adc.cpp

volatile bool LED_RS485_Flag    = false;
volatile bool LED_Module_Flag   = false;
volatile bool LED_SendOK_Flag   = false;

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
  M1_PowerOff();

  /* NBIOT PWR-ON */
  GPIO_InitStruct.Pin = ioNBIOT_PWR;
  LL_GPIO_Init(pNBIOT_PWR, &GPIO_InitStruct);
  LL_GPIO_SetOutputPin(pNBIOT_PWR, ioNBIOT_PWR);

  /* I2C protect pin */
  GPIO_InitStruct.Pin = ioEEPROMProtect;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(pEEPROMProtect, &GPIO_InitStruct);
  EEPROM_EnableProtect();

  /* NBIOT V-INT */
  GPIO_InitStruct.Pin = ioNBIOT_VINT;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(pNBIOT_VINT, &GPIO_InitStruct);

  // /* Detect ublox or EXS62 (old : NBIOT RESET-N) */
  // GPIO_InitStruct.Pin = ioDetectModule;
  // LL_GPIO_Init(pDetectModule, &GPIO_InitStruct);
}

void TaskLED(void *pvParameters) {
  for(;;) {
    if(adc_Flag) {
      LL_GPIO_SetOutputPin(pPowerLED, ioPowerLED);
      if(LED_RS485_Flag || LED_SendOK_Flag)   LL_GPIO_SetOutputPin( pReceiveLED, ioReceiveLED);
      if(LED_Module_Flag)  LL_GPIO_SetOutputPin( pSendLED, ioSendLED);

      if(LED_SendOK_Flag)  {vTaskDelay(pdMS_TO_TICKS(1000)); LED_SendOK_Flag = false;}
      else            vTaskDelay(pdMS_TO_TICKS(10));

      LL_GPIO_ResetOutputPin( pPowerLED, ioPowerLED);
      LL_GPIO_ResetOutputPin( pReceiveLED, ioReceiveLED);
      LL_GPIO_ResetOutputPin( pSendLED, ioSendLED);
    }

    vTaskDelay(pdMS_TO_TICKS(990));
  }
}