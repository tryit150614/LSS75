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
extern unsigned int lpuart1DataLength;
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


  return 1;
}

int sara_writeInFileSystem(char * payload, uint32_t len)
{
  /* Delete data and rp in file system */
  writeString(DEBUG_USART, "(UART) AT+UDELFILE=\"data1\"\r\n");
  writeString(LPUART1, "AT+UDELFILE=\"data1\"\r\n");
  sara_getResponse(1, NULL);
  
  /* Write data into file system */
  writeString(DEBUG_USART, "(UART) AT+UDWNFILE=\"data1\",");
  writeUint32(DEBUG_USART, len);
  writeString(DEBUG_USART, "\r\n");

  writeString(LPUART1, "AT+UDWNFILE=\"data1\",");
  writeUint32(LPUART1, len);
  writeString(LPUART1, "\r\n");

  sara_getResponse(1, NULL);

  HAL_Delay(1500);
  watchdog.reload();
  writeString(LPUART1, payload);  
  writeString(LPUART1, "\r\n");  

  sara_getResponse(1, NULL);
}

// void sendCmd(char * str)
// {
//     writeString(LPUART1, str);
// }

/*
return 1 : OK
       0 : Fail
*/
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