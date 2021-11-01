#include "SARA-R410M.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "main.h"
#include "serialHandlers.h"
#include "st_watchdog.h"
#include "usart.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "FreeRTOSConfig.h"

extern volatile ringBufferStruct lpuart1Buf;
extern struct inputString inputStringLpuart1;
extern int lpuart1DataLength;
extern uint8_t lpuart1Data[800];
extern uint8_t dmaLpuart1Buffer[DMA_LPUART1_BUFFER_SIZE];

extern Watchdog watchdog;
extern char pHttpAddr[100];

static char connectIP[] = "\"35.187.150.128\"";
static char connectPort[] = "80";
static char connectPath[100] = "\"/api/6th-nbiot\"";

int sara_ready()
{
    NBIOT_PowerOn();
    LL_GPIO_ResetOutputPin(pNBIOT_PWR, ioNBIOT_PWR);
    HAL_Delay(3200);
    LL_GPIO_SetOutputPin(pNBIOT_PWR, ioNBIOT_PWR);
    HAL_Delay(1300);

    return 1;
}

int sara_setHttp()
{
  // DEBUG("Enter init http\r\n");
  uint8_t result = 0;
  /* Setting http server IP */
  writeString(LPUART1, "AT+UHTTP=0,0,");
  writeString(LPUART1, connectIP);
  writeString(LPUART1, "\r\n");

  writeString(DEBUG_USART, "(UART) AT+UHTTP=0,0,");
  writeString(DEBUG_USART, connectIP);
  writeString(DEBUG_USART, "\r\n");

  if(!sara_getResponse(2, "OK")) return 0;

  /* Setting http server Port */
  writeString(LPUART1, "AT+UHTTP=0,5,");
  writeString(LPUART1, connectPort);
  writeString(LPUART1, "\r\n");

  writeString(DEBUG_USART, "(UART) AT+UHTTP=0,5,");
  writeString(DEBUG_USART, connectPort);
  writeString(DEBUG_USART, "\r\n");

  if(!sara_getResponse(2, "OK")) return 0;

  /* Setting http server header */
  writeString(DEBUG_USART, "(UART) AT+UHTTP=0,9,\"0:Content-Type:application/json\"\r\n");
  writeString(LPUART1, "AT+UHTTP=0,9,\"0:Content-Type:application/json\"\r\n");
  if(!sara_getResponse(2, "OK")) return 0;

  writeString(DEBUG_USART, "(UART) AT+UHTTP=0,9,\"1:Accept:application/json\"\r\n");
  writeString(LPUART1, "AT+UHTTP=0,9,\"1:Accept:application/json\"\r\n");
  if(!sara_getResponse(2, "OK")) return 0;
  
  /* 只掃Band 8, Band 28 */
  writeString(DEBUG_USART, "(UART) AT+UBANDMASK=1,134217856,0\r\n");
  writeString(LPUART1, "AT+UBANDMASK=1,134217856,0\r\n");
  if(!sara_getResponse(2, "OK")) return 0;

  return 1;
}

int sara_writeInFileSystem(char * payload, uint32_t len)
{
  writeString(DEBUG_USART, "(UART) AT+UDELFILE=\"rp1\"\r\n");
  writeString(LPUART1, "AT+UDELFILE=\"rp1\"\r\n");
  sara_getResponse(2, "OK");

  /* Delete data and rp in file system */
  writeString(DEBUG_USART, "(UART) AT+UDELFILE=\"data1\"\r\n");
  writeString(LPUART1, "AT+UDELFILE=\"data1\"\r\n");
  sara_getResponse(1, "OK");
  
  /* Write data into file system */
  writeString(DEBUG_USART, "(UART) AT+UDWNFILE=\"data1\",");
  writeUint32(DEBUG_USART, len);
  writeString(DEBUG_USART, "\r\n");

  writeString(LPUART1, "AT+UDWNFILE=\"data1\",");
  writeUint32(LPUART1, len);
  writeString(LPUART1, "\r\n");

  sara_getResponse(1, "OK");

  HAL_Delay(1500);
  watchdog.reload();
  writeString(LPUART1, payload);  
  writeString(LPUART1, "\r\n");  

  sara_getResponse(1, "OK");

  return 1;
}

int sara_getResponse(int sec, char* target)
{
    uint32_t start = HAL_GetTick();
    uint32_t current = HAL_GetTick();
    uint32_t timeout = sec * 1000;

    while ((current - start) < timeout)
    {
        watchdog.reload();
        current = HAL_GetTick();
        receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
        if (inputStringLpuart1.ready == 1)
        {
            inputStringLpuart1.ready = 0;
            char *part = strstr(inputStringLpuart1.string, target);
            if(part != NULL)
            {
                return 1; //OK
            }
        }
    }
    
    /* Fail */
    return 0;
}

int sara_http_send() {
    writeString(LPUART1, "AT+UHTTPC=0,4,");
    writeString(LPUART1, connectPath);
    writeString(LPUART1, ",\"rp1\",\"data1\",4\r\n");

    char msg[1000] = {0};
    uint32_t start = HAL_GetTick();
    uint32_t current = HAL_GetTick();
    uint32_t timeout = 3000;
    while ((current - start) < timeout)
    {
        watchdog.reload();
        current = HAL_GetTick();
        receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
        if (inputStringLpuart1.ready == 1)
        {
            inputStringLpuart1.ready = 0;
            strcat(msg, inputStringLpuart1.string);
        }
    }

    char *str      = strstr(msg, "OK");
    char *errorMsg = strstr(msg, "422 Unprocessable");
    if     (str      != NULL) return 0; // OK
    else if(errorMsg != NULL) return 1; // 422
    else                      return 2; // Other error
}

int sara_poweroff() {

    writeString(LPUART1, "AT+CPWROFF\r\n");

    uint32_t start = HAL_GetTick();
    uint32_t current = HAL_GetTick();
    uint32_t timeout = 10000;
    while ((current - start) < timeout)
    {
      watchdog.reload();
      current = HAL_GetTick();
      if (LL_GPIO_IsInputPinSet(pNBIOT_VINT, ioNBIOT_VINT) == 0)
      {
        DEBUG("V-INT is low, close power.\r\n");
        NBIOT_PowerOff();
        return 1;
      }
    }

    return 0;
}

int sara_checkIP() {
    writeString(LPUART1, "AT+CGDCONT?\r\n");

    if(sara_getResponse(1, "+CGDCONT: 1,\"IPV4V6\",\"\",\"") || strnlen(inputStringLpuart1.string, 35) <= 10)
    { /* 尚未註冊 */
      return 0;
    }
    else if(memcmp(inputStringLpuart1.string, "AT+CGDCONT?\r\r\n+CGDCONT: 1,\"IPV4V6\",", 35) == 0)
    { /* 已經註冊 */
      return 1;
    }
}

void sara_getRSSI(char * out) {
  writeString(LPUART1, "AT+CESQ\r\n");
  if(sara_getResponse(1, "+CESQ: ")) {
    char *str;
    strtok(inputStringLpuart1.string, ",");
    strtok(NULL, ",");
    strtok(NULL, ",");
    strtok(NULL, ",");
    strtok(NULL, ",");
    str = strtok(NULL, ",");
    if(memcmp(str, "99", 2) == 0 || memcmp(str, "255", 3) == 0) {
      out[0] = '0';
    } else {
      int rssiVal = strtol(str, NULL, 10) - 111;
      sprintf(out, "%d", rssiVal);
    }
  } else {
    out[0] = '0';
  }
}