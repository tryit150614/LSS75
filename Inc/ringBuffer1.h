#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#ifdef __cplusplus
extern "C" {
 #endif

#include <stdint.h>

/* Enums */
enum ringBufferStatus {BUFFER_OK, BUFFER_EMPTY, BUFFER_FULL};
/* Structs */
typedef struct ringBufferStruct {

  uint8_t * bufferPtr;
  unsigned int bufferLen;
  uint32_t writeIndex;
  uint32_t readIndex;

}ringBufferStruct;

/* Function prototypes */
void ringBuffer_new(volatile ringBufferStruct * ringBuffer, uint8_t * bufferArray, unsigned int buferrLen);
enum ringBufferStatus ringBuffer_write(volatile ringBufferStruct * buffer, uint8_t byte);
char ringBuffer_read(volatile ringBufferStruct * buffer);
int ringBuffer_hasData(volatile ringBufferStruct * buffer);
void ringBuffer_resetBuffer(volatile ringBufferStruct * buffer);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* RING_BUFFER_H */
