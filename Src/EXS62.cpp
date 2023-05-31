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

extern Watchdog watchdog;
extern char pHttpAddr[100];

int dbg_exs62_getResponse(int sec, const char* target);

int exs62_ready()
{
    /* Wait module ready */
    if(exs62_getResponse(15, "SYSSTART")) return 1;

    return 0;
}

int exs62_switchOff()
{
  /*傳隨一指令, 防止遇到其他指令正在處理而使^SMSO失敗*/
  sendCmd("AT\r\n");
  exs62_getResponse(1, "OK");
  
  sendCmd("AT^SMSO\r\n");
  if(exs62_getResponse(2, "OK")) return 1;

  HAL_Delay(2000);
  return 0;
}

int exs62_setHttp()
{
    /* Http setting */
    sendCmd("AT^SISS=6,srvType,\"Http\"\r\n");
    if(exs62_getResponse(2, "OK")) return 1;
    
    sendCmd("AT^SISS=6,conId,\"1\"\r\n");
    if(exs62_getResponse(2, "OK")) return 1;

    sendCmd("AT^SISS=6,\"address\",\"");
    sendCmd(pHttpAddr);
    sendCmd("\"\r\n");
    if(exs62_getResponse(2, "OK")) return 1;

    sendCmd("AT^SISS=6,cmd,\"post\"\r\n");
    if(exs62_getResponse(2, "OK")) return 1;

    sendCmd("AT^SISS=6,\"hcProp\",\"Content-Type: application/json\\0d\\0aAccept: application/json\"\r\n");
    if(exs62_getResponse(2, "OK")) return 1;

    return 0;
}

int exs62_connectNetwork() {
  /* 設定電信商的APN, 才可連網 */
    sendCmd("AT+CGDCONT=1,\"IPV4V6\",\"internet.iot\"\r\n");
    if(exs62_getResponse(2, "OK")) return 1;

    sendCmd("AT+CGDCONT=2,\"IPV4V6\",\"nbiot\"\r\n");
    if(exs62_getResponse(2, "OK")) return 1;

    /* 設定掃頻(NBIOT掃BAND 8 28, CatM掃BAND 3)*/
    sendCmd("AT^SCFG=\"Radio/Band/CatNB\",\"8000080\",\"0\"\r\n");
    if(exs62_getResponse(2, "OK")) return 1;

    sendCmd("AT^SCFG=\"Radio/Band/CatM\",\"4\",\"0\"\r\n");
    if(exs62_getResponse(2, "OK")) return 1;

    /* 確認是否已註冊網路 */
    bool networkFlag = false;
    uint32_t start = HAL_GetTick();
    uint32_t current = HAL_GetTick();
    const uint32_t timeout = 120000;
    while ((current - start) < timeout)
    {   
      watchdog.reload();
      current = HAL_GetTick();

      sendCmd("AT+COPS?\r\n");
      if(exs62_getResponse(1, "+COPS: 0,0,")) HAL_Delay(1000);
      else {networkFlag = true; break;}
    }
    if(!networkFlag) return 1;

    /* Internet connection activate */
    HAL_Delay(1000);
    sendCmd("AT^SICA=1,1\r\n");
    if(exs62_getResponse(5, "OK")) return 1;

    return 0;
}

void sendCmd(const char * str)
{
    writeString(LPUART1, str);
}

/*
return 0 : OK
       1 : Fail
*/
int exs62_getResponse(int sec, const char* target)
{
    uint32_t start = HAL_GetTick();
    uint32_t current = HAL_GetTick();
    uint32_t timeout = sec * 1000;
    bool isWorking = false;

    while ((current - start) <= timeout)
    {
        watchdog.reload();
        current = HAL_GetTick();
        receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
        if (inputStringLpuart1.ready == 1)
        {
            inputStringLpuart1.ready = 0;
            isWorking = true;
            char *part = strstr(inputStringLpuart1.string, target);
            if(part != NULL)
            {
                return 0; //OK
            }

            part = strstr(inputStringLpuart1.string, "422 Unprocessable Entity");
            if(part != NULL)
            {
                return 422;
            }

            part = strstr(inputStringLpuart1.string, "Remote host has reset the connection");
            if(part != NULL)
            {
                return 2; // Disconnect
            }

            part = strstr(inputStringLpuart1.string, "+CME ERROR: operation failed");
            if(part != NULL)
            {
                return 2; // Unknow error
            }
        }
    }
    
    /* Fail */
    if(isWorking) return 1;
    else          return 3; // NO response
}

int exs62_preparePayload_Connect(uint32_t len) {
  // 設定資料長度
  char cmd[100] = {0};
  sprintf(cmd, "AT^SISS=6,\"hcContLen\",\"%lu\"\r\n", len);
  sendCmd(cmd);
  if(exs62_getResponse(2, "OK")) return 1;

  // 開啟連線 + 寫資料 + 傳送
  sendCmd("AT^SISO=6\r\n");
  if(exs62_getResponse(10, "^SISW: 6,1")) return 1;

  return 0;
}

int exs62_closeConnetion() {
  sendCmd("AT^SISC=6\r\n");
  if(exs62_getResponse(2, "OK")) return 1;

  return 0;
}

int exs62_http_send(char *payload, uint32_t payloadlen) {
  char cmd[100] = {0};
  int result = 1;

  if(payloadlen <= 1500)
  {
    sprintf(cmd, "AT^SISW=6,%lu\r\n", payloadlen);
    sendCmd(cmd);
    result = dbg_exs62_getResponse(1, "^SISW: 6,");
    if(result) {DEBUG("SEND ERROR 1\r\n"); return result;}
    writeData(LPUART1, payload, payloadlen);
    result = dbg_exs62_getResponse(2, "SISW: 6,");
    if(result) {DEBUG("SEND ERROR 2\r\n"); return result;}
  }
  else
  {
    char * pData = payload;
    uint8_t count = payloadlen / 1500;
    uint32_t len = payloadlen;
    for(int i = 0; i < count; i++)
    {
      sendCmd("AT^SISW=6,1500\r\n");
      result = dbg_exs62_getResponse(1, "^SISW: 6,");
      if(result) {DEBUG("SEND ERROR 1\r\n"); return result;}
      HAL_Delay(50);
      writeData(LPUART1, pData, 1500);
      pData += 1500;
      len -= 1500;
      result = dbg_exs62_getResponse(2, "SISW: 6,");
      if(result) {DEBUG("SEND ERROR 2\r\n"); return result;}
      HAL_Delay(50);
    }

    HAL_Delay(50);
    sprintf(cmd, "AT^SISW=6,%lu\r\n", len);
    sendCmd(cmd);
    result = dbg_exs62_getResponse(1, "^SISW: 6,");
    if(result) {DEBUG("SEND ERROR 1\r\n"); return result;}
    HAL_Delay(50);
    writeData(LPUART1, pData, len);
    HAL_Delay(50);
    result = dbg_exs62_getResponse(1, "SISW: 6,");
    if(result) {DEBUG("SEND ERROR 2\r\n"); return result;}
  }

  return 0;
}

int exs62_getRSSI(char * out) {
  sendCmd("AT^SMONI\r\n");
  if(!exs62_getResponse(1, "CONN,")) {
    // RSRP於第12格
    char *str;
    strtok(inputStringLpuart1.string, ",");
    strtok(nullptr, ",");
    strtok(nullptr, ",");
    strtok(nullptr, ",");
    strtok(nullptr, ",");
    strtok(nullptr, ",");
    strtok(nullptr, ",");
    strtok(nullptr, ",");
    strtok(nullptr, ",");
    strtok(nullptr, ",");
    strtok(nullptr, ",");
    str = strtok(nullptr, ",");

    memcpy(out, str, strnlen(str, 4));
    return 0;
  } 
  else 
    return 1;
}

int dbg_exs62_getResponse(int sec, const char* target)
{
    uint32_t start = HAL_GetTick();
    uint32_t current = HAL_GetTick();
    uint32_t timeout = sec * 1000;
    static char tempStr[100] = {0};
    bool isWorking = false;

    while ((current - start) <= timeout)
    {
        watchdog.reload();
        current = HAL_GetTick();
        receiveBinaryInBuffer(&inputStringLpuart1, &lpuart1Buf);
        if (inputStringLpuart1.ready == 1)
        {
            inputStringLpuart1.ready = 0;
            isWorking = true;
            char final[300]={0};
            strcpy(final, tempStr);
            strcat(final, inputStringLpuart1.string);
            memset(tempStr, 0, sizeof(tempStr));
            strcpy(tempStr, inputStringLpuart1.string);
            char *part = strstr(final, target);
            if(part != NULL)
            {
                return 0; //OK
            }

            part = strstr(inputStringLpuart1.string, "422 Unprocessable Entity");
            if(part != NULL)
            {
                return 422;
            }

            part = strstr(inputStringLpuart1.string, "Remote host has reset the connection");
            if(part != NULL)
            {
                return 2; // Disconnect
            }

            part = strstr(inputStringLpuart1.string, "+CME ERROR: operation failed");
            if(part != NULL)
            {
                return 2; // Unknow error
            }
        }
    }
    
    /* Fail */
    if(isWorking) return 1;
    else          return 3; // NO response
}