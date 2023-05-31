/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ADC_H
#define __ADC_H

#define DMA1_ADC_BUFFER_SIZE 4

#include "stdint.h"

void MX_ADC1_Init(void);

void ADC_enable();
void ADC_enableInternalRegulator();
uint32_t ADC_isInternalRegulatorEnabled();
uint32_t ADC_isReady();
void ADC_startConversion();
float ADC_calculateTemperature(float v0);

void voltToTemperature(float tempVolt, char * tempStr);

void TaskADC(void * parameter);

#endif /*__ adc_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
