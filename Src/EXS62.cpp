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

extern volatile bool moduleFlag;
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
    /* Wait module ready */
    if(!exs62_getResponse(1, EXS62_ACK::bootupStr)) return 0;

    moduleFlag = true;
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

    return 1;
}

int exs62_connectNetwork() {
  /* 設定電信商的APN, 才可連網 */
    sendCmd("AT+CGDCONT=1,\"IPV4V6\",\"internet.iot\"\r\n");
    HAL_Delay(300);

    sendCmd("AT+CGDCONT=2,\"IPV4V6\",\"nbiot\"\r\n");
    HAL_Delay(300);

    /* 設定掃頻(NBIOT掃BAND 8 28, CatM掃BAND 3)*/
    sendCmd("AT^SCFG?\r\n");
    HAL_Delay(300);

    sendCmd("AT^SCFG=\"Radio/Band/CatNB\",\"8000080\",\"0\"\r\n");
    HAL_Delay(300);

    sendCmd("AT^SCFG=\"Radio/Band/CatM\",\"4\",\"0\"\r\n");
    HAL_Delay(300);

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

int exs62_preparePayload_Connect(uint32_t len) {
  // 設定資料長度
  char cmd[100] = {0};
  sprintf(cmd, "AT^SISS=6,\"hcContLen\",\"%lu\"\r\n", len);
  sendCmd(cmd);
  if(!exs62_getResponse(2, "OK")) {
    return 0;
  }

  // 開啟連線 + 寫資料 + 傳送
  sendCmd("AT^SISO=6\r\n");
  if(!exs62_getResponse(5, "^SISW: 6,1")) {
    return 0;
  }

  return 1;
}

int exs62_closeConnetion() {
  sendCmd("AT^SISC=6\r\n");
  if(!exs62_getResponse(2, "OK")) {
    return 0;
  }

  return 1;
}

int exs62_http_send(char *payload, uint32_t payloadlen) {
  char cmd[100] = {0};
  if(payloadlen <= 1500)
  {
    sprintf(cmd, "AT^SISW=6,%lu\r\n", payloadlen);
    sendCmd(cmd);
    HAL_Delay(50);
    sendCmd(payload);
  }
  else
  {
    char * pData = payload;
    uint8_t count = payloadlen / 1500;
    uint32_t len = payloadlen;
    for(int i = 0; i < count; i++)
    {
      sendCmd("AT^SISW=6,1500\r\n");
      HAL_Delay(50);
      writeData(LPUART1, pData, 1500);
      pData += 1500;
      len -= 1500;

      if(exs62_getResponse(3, "SISW: 6,1")) continue;
      else                                  return 0;
    }

    sprintf(cmd, "AT^SISW=6,%lu\r\n", len);
    sendCmd(cmd);
    HAL_Delay(50);
    writeData(LPUART1, pData, len);
  }

  return 1;
}