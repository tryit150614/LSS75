#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "nodeManager.h"
#include "usart.h"
#include "flashWrapper.h"
#include "main.h"

extern struct ResendStruct flashResend;
extern struct MACStruct flashMAC;
extern struct HashStruct flashHASH;
extern struct FlashFlagStruct flashFlag;
extern volatile uint32_t pResendTime;
extern volatile uint32_t pMAC;
extern char pHash[9];

struct ResendStruct tempResendStruct;
struct MACStruct tempMACStruct;
struct HashStruct tempHashStruct;
struct FlashFlagStruct tempFlashFlagStruct;
const unsigned long ResendStructSize = sizeof(flashResend);
const unsigned long MACStructSize = sizeof(flashMAC);
const unsigned long HashStructSize = sizeof(flashHASH);
const unsigned long FlashFlagStructSize = sizeof(flashFlag);

static uint64_t flashAccessBuffer[FLASH_ACCESS_BUFFER_SIZE];

void nodeManager_initFlashData() {
  memset(flashAccessBuffer, 0, sizeof(flashAccessBuffer));
  memcpy(flashAccessBuffer, &flashResend, ResendStructSize);
  flashWriteBlock_first(flashAccessBuffer, FLASH_RESEND_START_ADDR, ResendStructSize);

  memset(flashAccessBuffer, 0, sizeof(flashAccessBuffer));
  memcpy(flashAccessBuffer, &flashMAC, MACStructSize);
  flashWriteBlock_first(flashAccessBuffer, FLASH_MAC_START_ADDR, MACStructSize);

  memset(flashAccessBuffer, 0, sizeof(flashAccessBuffer));
  memcpy(flashAccessBuffer, &flashHASH, HashStructSize);
  flashWriteBlock_first(flashAccessBuffer, FLASH_HASH_START_ADDR, HashStructSize);
}

void nodeManager_riseInitFlag() {
  memset(flashAccessBuffer, 0, sizeof(flashAccessBuffer));
  memcpy(&tempFlashFlagStruct, flashAccessBuffer, FlashFlagStructSize);

  tempFlashFlagStruct.initFlag = 0;

  memset(flashAccessBuffer, 0, sizeof(flashAccessBuffer));
  memcpy(flashAccessBuffer, &tempFlashFlagStruct, FlashFlagStructSize);
  flashWriteBlock_with_erase(flashAccessBuffer, FLASH_FLAG_START_ADDR, FlashFlagStructSize, FLASH_FLAG_START_PAGE, FLASH_FLAG_END_PAGE);
}

void nodeManager_changeResendTime(uint64_t newResendTime){
   memset(flashAccessBuffer, 0, sizeof(flashAccessBuffer));
   memcpy(&tempResendStruct, flashAccessBuffer, ResendStructSize);

   tempResendStruct.resendTime = newResendTime;
   pResendTime = newResendTime;

   memset(flashAccessBuffer, 0, sizeof(flashAccessBuffer));
   memcpy(flashAccessBuffer, &tempResendStruct, ResendStructSize);
   flashWriteBlock_with_erase(flashAccessBuffer, FLASH_RESEND_START_ADDR, ResendStructSize, FLASH_RESEND_START_PAGE, FLASH_RESEND_END_PAGE);
}

void nodeManager_modifyMAC(uint64_t mac) {

  memset(flashAccessBuffer, 0, sizeof(flashAccessBuffer));
  memcpy(&tempMACStruct, flashAccessBuffer, MACStructSize);

  tempMACStruct.mac = mac;
  pMAC = tempMACStruct.mac;
  /* pHash is changed on cli.cpp */

  memset(flashAccessBuffer, 0, sizeof(flashAccessBuffer));
  memcpy(flashAccessBuffer, &tempMACStruct, MACStructSize);
  flashWriteBlock_with_erase(flashAccessBuffer, FLASH_MAC_START_ADDR, MACStructSize, FLASH_MAC_START_PAGE, FLASH_MAC_END_PAGE);
}

void nodeManager_modifyHash(char * newHash) {

  memset(flashAccessBuffer, 0, sizeof(flashAccessBuffer));
  memcpy(&tempHashStruct, flashAccessBuffer, HashStructSize);

  memset(tempHashStruct.hash, 0, 8);
  memcpy(tempHashStruct.hash, newHash, 8);

  memset(pHash, 0, 8);
  memcpy(pHash, newHash, 8);
  /* pHash is changed on cli.cpp */

  memset(flashAccessBuffer, 0, sizeof(flashAccessBuffer));
  memcpy(flashAccessBuffer, &tempHashStruct, HashStructSize);
  flashWriteBlock_with_erase(flashAccessBuffer, FLASH_HASH_START_ADDR, HashStructSize, FLASH_HASH_START_PAGE, FLASH_HASH_END_PAGE);
}