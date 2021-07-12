#ifndef NODEMANAGER_H_
#define NODEMANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "main.h"

void nodeManager_initFlashData();
void nodeManager_riseInitFlag();

void nodeManager_modifyResendCount(uint64_t add);
void nodeManager_changeResendTime(uint64_t newResendTime);
void nodeManager_modifyMAC(uint64_t mac);
void nodeManager_modifyHash(char * newHash);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* NODEMANAGER_H_ */
