#include "serialHandlers.h"
#include "ringBuffer1.h"
#include "usart.h"
#include <string.h>

extern unsigned int usart1DataLength;
extern unsigned int lpuart1DataLength;

struct inputString inputStringLpuart1  = {{0}, 0, 0, 0, '\n'};
struct inputString inputStringUsart1   = {{0}, 0, 0, 0, '\n'};
struct inputString inputStringUsart2   = {{0}, 0, 0, 0, '\n'};
struct inputString inputStringUsart3   = {{0}, 0, 0, 0, '\n'};
struct inputString inputStringUart4    = {{0}, 0, 0, 0, '\n'};

const unsigned int lpuart1DataSize = 800;
uint8_t lpuart1Data[lpuart1DataSize];
volatile ringBufferStruct lpuart1Buf;

const unsigned int usart1DataSize = 800;
uint8_t usart1Data[usart1DataSize];
volatile ringBufferStruct usart1Buf;

const unsigned int usart2DataSize = 200;
uint8_t usart2Data[usart2DataSize];
volatile ringBufferStruct usart2Buf;

const unsigned int usart3DataSize = 200;
uint8_t usart3Data[usart3DataSize];
volatile ringBufferStruct usart3Buf;

const unsigned int uart4DataSize = 240;
uint8_t uart4Data[uart4DataSize];
volatile ringBufferStruct uart4Buf;

void parseStringInBuffer(struct inputString * strIn, volatile ringBufferStruct * ringBuffer) {
  
  if (ringBuffer_hasData(ringBuffer)) {
    while (ringBuffer_hasData(ringBuffer) && (strIn->ready == 0)) {
      char rc = ringBuffer_read(ringBuffer);
      char endMarker = strIn->endMarker;

      if ((rc != endMarker)) {
        if (strIn->counter >= ringBuffer->bufferLen) {
          strIn->counter = ringBuffer->bufferLen - 1;
        }

        strIn->string[strIn->counter] = rc;
        strIn->counter++;
      }
      else {
        strIn->string[strIn->counter] = '\n';
        strIn->string[strIn->counter+1] = '\0';
        strIn->counter = 0;
        strIn->ready = 1;
      }
    }
  }
}

void parseHexInBuffer(struct inputString * strIn, volatile ringBufferStruct * ringBuffer, int len) {

  int i = 0;
  if (ringBuffer_hasData(ringBuffer)) {
    while (strIn->ready == 0) {

      char rc = ringBuffer_read(ringBuffer);
      if (i < len ) {
        if (strIn->counter >= ringBuffer->bufferLen) {
          strIn->counter = ringBuffer->bufferLen - 1;
        }

        strIn->string[strIn->counter] = rc;
        strIn->counter++;
        i++;
      }
      else {
        strIn->string[strIn->counter] = '\0';
        strIn->counter = 0;
        strIn->ready = 1;
      }
    }
  }
}

void sendBinaryInBuffer(struct inputString * strIn, volatile ringBufferStruct * ringBuffer) {
  if (ringBuffer_hasData(ringBuffer)) {
    while (ringBuffer_hasData(ringBuffer) && (strIn->ready == 0)) {
      char rc = ringBuffer_read(ringBuffer);

      if (strIn->counter < (usart1DataLength - 1)) {
        if (strIn->counter >= ringBuffer->bufferLen) {
          strIn->counter = ringBuffer->bufferLen - 1;
        }

        strIn->string[strIn->counter] = rc;
        strIn->counter++;
      }
      else {
        strIn->string[strIn->counter] = '\0';
        strIn->counter = 0;
        strIn->ready = 1;
        usart1DataLength = 0;
      }
    }
  }
}

void receiveBinaryInBuffer(struct inputString * strIn, volatile ringBufferStruct * ringBuffer) {
  int i = 0;
  if (ringBuffer_hasData(ringBuffer)) {
    while ((strIn->ready == 0)) {
      char rc = ringBuffer_read(ringBuffer);

      if (i < (lpuart1DataLength + 0) ) {
        if (strIn->counter >= ringBuffer->bufferLen) {
          strIn->counter = ringBuffer->bufferLen - 1;
        }

        strIn->string[strIn->counter] = rc;
        strIn->counter++;
        i++;
      }
      else {
        strIn->string[strIn->counter] = '\0';
        strIn->counter = 0;
        strIn->ready = 1;
        lpuart1DataLength = 0;
      }
    }
  }
}

void ringBuffer_init() {
  ringBuffer_new(&lpuart1Buf, lpuart1Data, lpuart1DataSize);
  ringBuffer_new(&usart1Buf, usart1Data, usart1DataSize);
  ringBuffer_new(&uart4Buf, uart4Data, uart4DataSize);
  ringBuffer_new(&usart2Buf, usart2Data, usart2DataSize);
  ringBuffer_new(&usart3Buf, usart3Data, usart3DataSize);
}