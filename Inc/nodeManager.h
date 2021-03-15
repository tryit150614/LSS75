#ifndef NODEMANAGER_H_
#define NODEMANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "main.h"

void nodeManager_initFlashData(uint8_t userInit, uint8_t flagInit);
void nodeManager_riseInitFlag();

void nodeManager_modifyResendCount(uint64_t add);
void nodeManager_changeResendTime(uint64_t newResendTime);
void nodeManager_modifyMACandHash(uint64_t mac, uint64_t hash1, uint64_t hash2);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* NODEMANAGER_H_ */
