/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_ll_adc.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_dma.h"
#include "adc.h"
#include <math.h>

uint16_t adc1DmaBuffer[DMA1_ADC_BUFFER_SIZE] = {0};

void MX_ADC1_Init(void)
{
  LL_ADC_InitTypeDef ADC_InitStruct = {0};
  LL_ADC_REG_InitTypeDef ADC_REG_InitStruct = {0};
  LL_ADC_CommonInitTypeDef ADC_CommonInitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Peripheral clock enable */
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_ADC);
  
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
  /**ADC1 GPIO Configuration  
  PC1   ------> ADC1_IN2
  PC0   ------> ADC1_IN1
  PB1   ------> ADC1_IN16 
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_1|LL_GPIO_PIN_0;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_1;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* ADC1 DMA Init */
  
  /* ADC1 Init */
LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMA_REQUEST_0);
LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_1, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PRIORITY_LOW);
LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MODE_CIRCULAR);
LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PERIPH_NOINCREMENT);
LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MEMORY_INCREMENT);
LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PDATAALIGN_HALFWORD);
LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MDATAALIGN_HALFWORD);

LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_1, LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA), (uint32_t)adc1DmaBuffer, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, DMA1_ADC_BUFFER_SIZE);
// LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

/** Common config
*/
ADC_InitStruct.Resolution = LL_ADC_RESOLUTION_12B;
ADC_InitStruct.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
ADC_InitStruct.LowPowerMode = LL_ADC_LP_MODE_NONE;
LL_ADC_Init(ADC1, &ADC_InitStruct);
ADC_REG_InitStruct.TriggerSource = LL_ADC_REG_TRIG_SOFTWARE;
ADC_REG_InitStruct.SequencerLength = LL_ADC_REG_SEQ_SCAN_ENABLE_4RANKS;
ADC_REG_InitStruct.SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE;
ADC_REG_InitStruct.ContinuousMode = LL_ADC_REG_CONV_SINGLE;
ADC_REG_InitStruct.DMATransfer = LL_ADC_REG_DMA_TRANSFER_UNLIMITED;
ADC_REG_InitStruct.Overrun = LL_ADC_REG_OVR_DATA_OVERWRITTEN;
LL_ADC_REG_Init(ADC1, &ADC_REG_InitStruct);
LL_ADC_DisableIT_EOC(ADC1);
LL_ADC_DisableIT_EOS(ADC1);
LL_ADC_DisableDeepPowerDown(ADC1);
LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1), LL_ADC_PATH_INTERNAL_VREFINT);
LL_ADC_EnableInternalRegulator(ADC1);
while(ADC_isInternalRegulatorEnabled() == 0);
// HAL_Delay(10);
LL_ADC_StartCalibration(ADC1, LL_ADC_SINGLE_ENDED);
while(LL_ADC_IsCalibrationOnGoing(ADC1) == 1);
// HAL_Delay(10);
ADC_CommonInitStruct.CommonClock = LL_ADC_CLOCK_ASYNC_DIV1;
LL_ADC_CommonInit(__LL_ADC_COMMON_INSTANCE(ADC1), &ADC_CommonInitStruct);
LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);
LL_ADC_Enable(ADC1);
while(LL_ADC_IsActiveFlag_ADRDY(ADC1) == 0);

  /** Battery vatagle (PC0) 
  */
  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_1);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_1, LL_ADC_SAMPLINGTIME_47CYCLES_5);
  LL_ADC_SetChannelSingleDiff(ADC1, LL_ADC_CHANNEL_1, LL_ADC_SINGLE_ENDED);
  /** Solar votagle (PC1) 
  */
  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, LL_ADC_CHANNEL_2);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_2, LL_ADC_SAMPLINGTIME_47CYCLES_5);
  LL_ADC_SetChannelSingleDiff(ADC1, LL_ADC_CHANNEL_2, LL_ADC_SINGLE_ENDED);
  /** Internal tempurature votagle (PB1)
  */
  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_3, LL_ADC_CHANNEL_16);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_16, LL_ADC_SAMPLINGTIME_640CYCLES_5);
  LL_ADC_SetChannelSingleDiff(ADC1, LL_ADC_CHANNEL_16, LL_ADC_SINGLE_ENDED);
  /* Internal voltage reference -> adc1DmaBuffer[3] */
  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_4, LL_ADC_CHANNEL_VREFINT);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_VREFINT, LL_ADC_SAMPLINGTIME_47CYCLES_5);
  LL_ADC_SetChannelSingleDiff(ADC1, LL_ADC_CHANNEL_VREFINT, LL_ADC_SINGLE_ENDED);
}



void ADC_startConversion() {
  LL_ADC_REG_StartConversion(ADC1);
}

void ADC_enable() {
  LL_ADC_Enable(ADC1);
}

void ADC_enableInternalRegulator() {
  LL_ADC_EnableInternalRegulator(ADC1);
}

uint32_t ADC_isInternalRegulatorEnabled() {
  if(LL_ADC_IsInternalRegulatorEnabled(ADC1) == 0)
    return 0;
  else
    return 1;
}

uint32_t ADC_isReady() {
  if(LL_ADC_IsActiveFlag_ADRDY(ADC1) == 0)
    return 0;
  else
    return 1;
}

float ADC_calculateTemperature(float v0){
  float r0 = 10000, r1 = 0;
  float t0 = 0;

  r1 = (2.8 * r0 / v0) - r0;
  t0 = (1 / 298.15) + (log(r1 / r0) / 3435);


  return (1 / t0) - 273.15;
}
