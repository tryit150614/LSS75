#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <stdio.h>

#include "main.h"
#include "usart.h"
#include "st_rtc.h"
#include "rtcController.h"
#include "ringBuffer1.h"
#include "serialHandlers.h"

#include "FreeRTOS.h"
#include "timers.h"
#include "semphr.h"
#include "stm32l4xx_ll_pwr.h"

extern volatile ringBufferStruct uart4Buf;
extern struct inputString inputStringUart4;

void TaskCalibrateRtc(void *pvParameters)
{
  Rtc *rtc = (Rtc *)pvParameters;
  char gprmcStr[200] = {0};

  // UART4_enableInterrupts();
  constexpr uint32_t pauseTime = 2000;
  for (;;)
  {
      // UART4_enableInterrupts();
      unsigned int stringFound = 0;
      uint32_t startT  = HAL_GetTick();
      uint32_t nowT    = HAL_GetTick();
      uint32_t timeout = 2000;

      while (stringFound != 1 && (nowT - startT) < timeout)
      {
          nowT = HAL_GetTick();
          parseStringInBuffer(&inputStringUart4, &uart4Buf);
          if (inputStringUart4.ready == 1)
          { 
            inputStringUart4.ready = 0;
            // writeString(DEBUG_USART, "in = ");writeString(DEBUG_USART, inputStringUart4.string);
            // writeString(DEBUG_USART, "\r\n");

            char gpsGPRMCHeader[] = "$GPRMC";
            unsigned int gpsHeaderLen = strnlen(gpsGPRMCHeader, 6);
            int cmpRMC = strncmp(inputStringUart4.string, gpsGPRMCHeader, gpsHeaderLen);

            if (stringFound == 0 && cmpRMC == 0 && inputStringUart4.string[17] == 'A')
            {
              // writeString(DEBUG_USART, "in = ");writeString(DEBUG_USART, inputStringUart4.string);
              // writeString(DEBUG_USART, "\r\n");
              stringFound = 1;

              memset(gprmcStr, 0, sizeof(gprmcStr));
              memcpy(gprmcStr, inputStringUart4.string, strnlen(inputStringUart4.string, 200));
            }

            memset(inputStringUart4.string, 0, sizeof(inputStringUart4.string));
          }
      }
      // UART4_disableInterrupts();

      /* Calibrate RTC time */
      if(stringFound == 1) {
          char * pch;
          strtok(gprmcStr, ",");

          // get time in the format hhmmss
          pch = strtok(nullptr, ",");

          char hourStr[2 + 1] = {0};
          char minuteStr[2 + 1] = {0};
          char secondStr[2 + 1] = {0};

          if (pch != nullptr)
          {
            memcpy(hourStr, pch, 2);
            memcpy(minuteStr, pch + 2, 2);
            memcpy(secondStr, pch + 4, 2);
          }

          // skip the tokens that we don't use
          strtok(nullptr, ",");
          strtok(nullptr, ",");
          strtok(nullptr, ",");
          strtok(nullptr, ",");
          strtok(nullptr, ",");
          strtok(nullptr, ",");

          // get ddmmyy
          pch = strtok(nullptr, ",");

          // get date in the format ddmmyy
          char dayStr[2 + 1] = {0};
          char monthStr[2 + 1] = {0};
          char yearStr[2 + 1] = {0};

          if (pch != nullptr)
          {
            memcpy(dayStr, pch, 2);
            memcpy(monthStr, pch + 2, 2);
            memcpy(yearStr, pch + 4, 2);
          }

          int year = ((int)yearStr[0] - 48) * 10 + (int)yearStr[1] - 48;
          int month = ((int)monthStr[0] - 48) * 10 + (int)monthStr[1] - 48;
          int day = ((int)dayStr[0] - 48) * 10 + (int)dayStr[1] - 48;

          int hour = ((int)hourStr[0] - 48) * 10 + (int)hourStr[1] - 48;
          int minute = ((int)minuteStr[0] - 48) * 10 + (int)minuteStr[1] - 48;
          int second = ((int)secondStr[0] - 48) * 10 + (int)secondStr[1] - 48;

          // update rtc
          if (LL_PWR_IsEnabledBkUpAccess() == 0)
            LL_PWR_EnableBkUpAccess();

          rtc->setTime(hour, minute, second);
          rtc->setDate(year, month, day);

          char msg[100] = {0};
          snprintf(msg, 100, "******* Change time to 20%02d/%02d/%02d %02d:%02d:%02d *******\r\n", year, month, day, hour, minute, second);
          writeString(DEBUG_USART, msg);

          LL_PWR_DisableBkUpAccess();
      }

      const TickType_t xdelayIdle = pdMS_TO_TICKS(pauseTime);
      vTaskDelay(xdelayIdle);
  }
}