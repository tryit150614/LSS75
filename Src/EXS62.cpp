#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "EXS62.h"
#include "main.h"
#include "serialHandlers.h"
#include "st_watchdog.h"
#include "usart.h"


extern volatile ringBufferStruct lpuart1Buf;
extern struct inputString inputStringLpuart1;
extern unsigned int lpuart1DataLength;
extern uint8_t lpuart1Data[800];
extern uint8_t dmaLpuart1Buffer[DMA_LPUART1_BUFFER_SIZE];

extern Watchdog watchdog;
extern char pHttpAddr[100];

volatile bool networkFlag = false;

namespace EXS62_ACK{
    char bootupStr[] = "\r\n^SYSSTART";
    char srvType[]   = "AT^SISS=6,srvType,\"Http\"\r\r\nOK";
    char conId[]     = "AT^SISS=6,conId,\"1\"\r\r\nOK";
    char cmd[]       = "AT^SISS=6,cmd,\"post\"\r\r\nOK";
    char header[]    = "";
}

int exs62_ready()
{
    // /* Enable EXS62's power */
    // NBIOT_PowerOff();
    // // M1_PowerOn();
    // HAL_Delay(1000);
    // memset(dmaLpuart1Buffer, 0, sizeof(dmaLpuart1Buffer));
    // memset(lpuart1Data, 0, sizeof(lpuart1Data));
    // ringBuffer_resetBuffer(&lpuart1Buf);

    /* Wait module ready */
    if(!exs62_getResponse(2, EXS62_ACK::bootupStr)) return 0;

    return 1;
}

int exs62_setHttp()
{
    /* Http setting */
    sendCmd("AT^SISS=6,srvType,\"Http\"\r\n");
    if(!exs62_getResponse(5, EXS62_ACK::srvType)) return 0;
    
    sendCmd("AT^SISS=6,conId,\"1\"\r\n");
    if(!exs62_getResponse(5, EXS62_ACK::conId)) return 0;

    sendCmd("AT^SISS=6,\"address\",\"");
    sendCmd(pHttpAddr);
    sendCmd("\"\r\n");
    if(!exs62_getResponse(5, "OK")) return 0;

    sendCmd("AT^SISS=6,cmd,\"post\"\r\n");
    if(!exs62_getResponse(5, EXS62_ACK::cmd)) return 0;

    sendCmd("AT^SISS=6,\"hcProp\",\"Content-Type: application/json\\0d\\0aAccept: application/json\"\r\n");
    if(!exs62_getResponse(5, "OK")) return 0;

    /* 確認是否已註冊網路 */
    uint32_t start = HAL_GetTick();
    uint32_t current = HAL_GetTick();
    const uint32_t timeout = 120000;
    while ((current - start) < timeout)
    {   
      watchdog.reload();
      current = HAL_GetTick();

      sendCmd("AT+COPS?\r\n");
      receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
      if (inputStringLpuart1.ready == 1)
      {
        inputStringLpuart1.ready = 0;
        char *part = strstr(inputStringLpuart1.string, "+COPS: 0,0,");
        if(part != NULL)
        {
          networkFlag = true;
          break;
        }

        HAL_Delay(2000);
      }
    }
    if(!networkFlag) return 0;

    /* Internet connection activate */
    HAL_Delay(1000);
    sendCmd("AT^SICA=1,1\r\n");
    if(!exs62_getResponse(5, "OK")) return 0;

    return 1;
}

void sendCmd(char * str)
{
    writeString(LPUART1, str);
}

/*
return 1 : OK
       0 : Fail
*/
int exs62_getResponse(int sec, char* target)
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

int exs62_getResponse_safety(int sec, char* target)
{
    uint32_t start = HAL_GetTick();
    uint32_t current = HAL_GetTick();
    uint32_t timeout = sec * 1000;
    char msg[1000] = {0};

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

    char *part = strstr(msg, target);
    if(part != NULL)
    {
        return 1; //OK
    }

    /* Fail */
    return 0;
}