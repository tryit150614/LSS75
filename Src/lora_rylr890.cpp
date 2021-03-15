
// #include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "lora_rylr890.h"
#include "usart.h"
#include "main.h"
#include "serialHandlers.h"

namespace rylr890{
  char packetReceivedHeader[] = "+RCV";
}

extern struct inputString inputStringUsart2;
extern volatile ringBufferStruct usart2Buf;

extern volatile uint32_t pLora_p;
extern volatile uint32_t pLora_b;
extern volatile uint32_t pLora_n;
extern volatile uint32_t pMAC;

int lora_setParams() {
  int res = 0;

  char lora_p_str[9] = {0};
  sprintf(lora_p_str, "%lu", pLora_p);
  writeString(LORA_USART, "AT+PARAMETER=");
  sendChar(LORA_USART, lora_p_str[0]);
  sendChar(LORA_USART, lora_p_str[1]);
  sendChar(LORA_USART, ',');
  sendChar(LORA_USART, lora_p_str[2]);
  sendChar(LORA_USART, lora_p_str[3]);
  sendChar(LORA_USART, ',');
  sendChar(LORA_USART, lora_p_str[4]);
  sendChar(LORA_USART, lora_p_str[5]);
  sendChar(LORA_USART, ',');
  sendChar(LORA_USART, lora_p_str[6]);
  sendChar(LORA_USART, lora_p_str[7]);
  writeString(LORA_USART, "\r\n");

  uint32_t start = HAL_GetTick();
  uint32_t current = HAL_GetTick();
  uint32_t timeout = 2000;
  while((current - start) < timeout) {
    current = HAL_GetTick();
    parseStringInBuffer(&inputStringUsart2, &usart2Buf);
    if(inputStringUsart2.ready == 1) {
      inputStringUsart2.ready = 0;
      if(strncmp(inputStringUsart2.string, "+OK\r", INPUT_BUFFER_SIZE) == 0) {
        writeString(DEBUG_USART, "Lora_1\r\n");
        res += 1;
        break;
      }
    }
  }

  writeString(LORA_USART, "AT+BAND=");
  writeUint32(LORA_USART, pLora_b);
  writeString(LORA_USART, "\r\n");

  start = HAL_GetTick();
  current = HAL_GetTick();
  while((current - start) < timeout) {
    current = HAL_GetTick();
    parseStringInBuffer(&inputStringUsart2, &usart2Buf);
    if(inputStringUsart2.ready == 1) {
      inputStringUsart2.ready = 0;
      if(strncmp(inputStringUsart2.string, "+OK\r", INPUT_BUFFER_SIZE) == 0) {
        writeString(DEBUG_USART, "Lora_2\r\n");
        res += 1;
        break;
      }
    }
  }

  writeString(LORA_USART, "AT+NETWORKID=");
  writeUint32(LORA_USART, pLora_n);
  writeString(LORA_USART, "\r\n");

  start = HAL_GetTick();
  current = HAL_GetTick();
  while((current - start) < timeout) {
    current = HAL_GetTick();
    parseStringInBuffer(&inputStringUsart2, &usart2Buf);
    if(inputStringUsart2.ready == 1) {
      inputStringUsart2.ready = 0;
      if(strncmp(inputStringUsart2.string, "+OK\r", INPUT_BUFFER_SIZE) == 0) {
        writeString(DEBUG_USART, "Lora_3\r\n");
        res += 1;
        break;
      }
    }
  }

  DEBUG("pMAC = ");writeUint32(DEBUG_USART, pMAC);DEBUG("\r\n");
  HAL_Delay(100);
  
  writeString(LORA_USART, "AT+ADDRESS=");
  writeUint32(LORA_USART, pMAC);
  writeString(LORA_USART, "\r\n");

  start = HAL_GetTick();
  current = HAL_GetTick();
  while((current - start) < timeout) {
    current = HAL_GetTick();
    parseStringInBuffer(&inputStringUsart2, &usart2Buf);
    if(inputStringUsart2.ready == 1) {
      inputStringUsart2.ready = 0;
      DEBUG("->");DEBUG(inputStringUsart2.string);DEBUG("\r\n");
      if(strncmp(inputStringUsart2.string, "+OK\r", INPUT_BUFFER_SIZE) == 0) {
        writeString(DEBUG_USART, "Lora_4\r\n");
        res += 1;
        break;
      }
    }
  }

  if(res == 3) {
    writeString(LORA_USART, "AT+ADDRESS=");
    writeUint32(LORA_USART, pMAC);
    writeString(LORA_USART, "\r\n");

    start = HAL_GetTick();
    current = HAL_GetTick();
    while((current - start) < timeout) {
      current = HAL_GetTick();
      parseStringInBuffer(&inputStringUsart2, &usart2Buf);
      if(inputStringUsart2.ready == 1) {
        inputStringUsart2.ready = 0;
        DEBUG("->");DEBUG(inputStringUsart2.string);DEBUG("\r\n");
        if(strncmp(inputStringUsart2.string, "+OK\r", INPUT_BUFFER_SIZE) == 0) {
          writeString(DEBUG_USART, "Lora_4\r\n");
          res += 1;
          break;
        }
      }
    }
  }
  
  if(res == 4){
    return 0;  
  }
  return 1;
}

int lora_sendData(char * DeviceNetworkID, char * data) {
  uint8_t len = strnlen(data, RYLR890_PAYLOAD_MAX_LEN);
  writeString(DEBUG_USART, "LEN = ");writeUint32(DEBUG_USART, len);writeString(DEBUG_USART, "\r\n");
  char lenStr[3+1] = {0};
  lenStr[0] = len / 100 + 0x30;
  lenStr[1] = (len % 100) / 10 + 0x30;
  lenStr[2] = (len % 10) + 0x30;
  lenStr[3] = '\0';

  // char msg[200 + 1] = {0};
  // sprintf(msg, "AT+SEND=%s,%s,%s\r\n", DeviceNetworkID, lenStr, data);
  // writeString(LORA_USART, msg);

  writeString(LORA_USART,"AT+SEND=");
  writeString(LORA_USART, DeviceNetworkID);
  sendChar(LORA_USART, ',');
  writeString(LORA_USART, lenStr);
  sendChar(LORA_USART, ',');
  writeData(LORA_USART, data, len);
  writeString(LORA_USART,"\r\n");

  return 0;
}

int lora_getPacket(char * in, char * payload) {

  char * pch, * remainStr;

  // +RCV=
  pch = strtok_r(in, ",", &remainStr);
  if(pch == nullptr) {return 1;}

  // dataLen
  pch = strtok_r(nullptr, ",", &remainStr);
  if(pch == nullptr) {return 1;}
  unsigned int dataLen = strtol(pch, NULL, 10);

  // payload
  strncpy(payload, remainStr, dataLen);
  return 0;
}
