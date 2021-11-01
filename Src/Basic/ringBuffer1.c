#include <stdint.h>
#include "ringBuffer1.h"
#include "usart.h"

enum ringBufferStatus ringBuffer_write(volatile ringBufferStruct * buffer, uint8_t byte) {

    uint32_t next_index = (((buffer->writeIndex)+1) % buffer->bufferLen);

    // if (next_index == buffer->readIndex){
    //   return BUFFER_FULL;
    // }

    buffer->bufferPtr[buffer->writeIndex] = byte;
    buffer->writeIndex = next_index;

    return BUFFER_OK;
}

char ringBuffer_read(volatile ringBufferStruct * buffer) {

    if (buffer->writeIndex != buffer->readIndex){

      uint8_t byte = buffer->bufferPtr[buffer->readIndex];
      buffer->readIndex = ((buffer->readIndex+1) % buffer->bufferLen);

      return byte;

    }

    return 0;

}

int ringBuffer_hasData(volatile ringBufferStruct * buffer) {

  unsigned int result = (buffer->bufferLen + buffer->writeIndex - buffer->readIndex) % buffer->bufferLen;

  if(result > 0) {
    return 1;

  }
  else {
    return 0;

  }

}

void ringBuffer_resetBuffer(volatile ringBufferStruct * buffer) {
	buffer->readIndex = 0;
	buffer->writeIndex = 0;
}

void ringBuffer_new(volatile ringBufferStruct * ringBuffer, uint8_t * bufferArray, unsigned int bufferLen) {
  ringBuffer->bufferPtr = bufferArray;
  ringBuffer->bufferLen = bufferLen;
  ringBuffer->writeIndex = 0;
  ringBuffer->readIndex = 0;
}
