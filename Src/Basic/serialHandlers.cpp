#include "serialHandlers.h"
#include "ringBuffer1.h"
#include "usart.h"
#include <string.h>

struct inputString inputStringLpuart1  = {{0}, 0, 0, 0, '\n'};
struct inputString inputStringUsart1   = {{0}, 0, 0, 0, '\n'};
struct inputString inputStringUsart2   = {{0}, 0, 0, 0, '\n'};
struct inputString inputStringUsart3   = {{0}, 0, 0, 0, '\n'};
struct inputString inputStringUart4    = {{0}, 0, 0, 0, '\n'};

extern int usart1DataLength;
extern int lpuart1DataLength;

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
        strIn->string[strIn->counter] = '\0';
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