#ifndef _SERIAL_HANDLER_
#define _SERIAL_HANDLER_

#include "ringBuffer1.h"

#define INPUT_BUFFER_SIZE 900

struct inputString {
  char string[INPUT_BUFFER_SIZE];
  unsigned int counter;
  uint8_t ready;
  char startMarker;
  char endMarker;
};

void parseStringInBuffer(struct inputString *strIn, volatile ringBufferStruct *ringBuffer);
void parseHexInBuffer(struct inputString * strIn, volatile ringBufferStruct * ringBuffer, int len);

void sendBinaryInBuffer(struct inputString * strIn, volatile ringBufferStruct * ringBuffer);
void receiveBinaryInBuffer(struct inputString * strIn, volatile ringBufferStruct * ringBuffer);

#endif /* _SERIAL_HANDLER_ */
