#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "nodeManager.h"
#include "usart.h"
#include "flashWrapper.h"
#include "main.h"

extern volatile uint32_t flashDataAddr;
extern volatile uint32_t flashMACAddr;
extern struct FlashDataStruct flashData;
extern struct FlashMACStruct flashMAC;
extern struct FlashFlagStruct flashFlag;
extern volatile uint32_t pResendTime;
extern volatile uint32_t pMAC;
extern char pHash[9];

struct FlashDataStruct tempFlashDataStruct;
struct FlashMACStruct tempFlashMACStruct;
struct FlashFlagStruct tempFlashFlagStruct;
const unsigned long FlashDataStructSize = sizeof(flashData);
const unsigned long FlashMACStructSize = sizeof(flashMAC);
const unsigned long FlashFlagStructSize = sizeof(flashFlag);

static uint64_t flashAccessBuffer[FLASH_ACCESS_BUFFER_SIZE];

void nodeManager_initFlashData(uint8_t userInit, uint8_t flagInit) {
  if(userInit) {
    memset(flashAccessBuffer, 0, sizeof(flashAccessBuffer));
    memcpy(flashAccessBuffer, &flashData, FlashDataStructSize);
    flashWriteBlock_first(flashAccessBuffer, flashDataAddr, FlashDataStructSize);
  }

  if(flagInit) {
    memset(flashAccessBuffer, 0, sizeof(flashAccessBuffer));
    memcpy(flashAccessBuffer, &flashMAC, FlashMACStructSize);
    flashWriteBlock_first(flashAccessBuffer, flashMACAddr, FlashMACStructSize);
  }
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
   memcpy(&tempFlashDataStruct, flashAccessBuffer, FlashDataStructSize);

   tempFlashDataStruct.resendTime = newResendTime;
   pResendTime = newResendTime;

   memset(flashAccessBuffer, 0, sizeof(flashAccessBuffer));
   memcpy(flashAccessBuffer, &tempFlashDataStruct, FlashDataStructSize);
   flashWriteBlock_with_erase(flashAccessBuffer, FLASH_USER_START_ADDR, FlashDataStructSize, FLASH_USER_START_PAGE, FLASH_USER_END_PAGE);
}

void nodeManager_modifyMACandHash(uint64_t mac, uint64_t hash1, uint64_t hash2) {

  memset(flashAccessBuffer, 0, sizeof(flashAccessBuffer));
  memcpy(&tempFlashMACStruct, flashAccessBuffer, FlashMACStructSize);

  tempFlashMACStruct.mac = mac;
  tempFlashMACStruct.hash1 = hash1;
  tempFlashMACStruct.hash2 = hash2;

  pMAC = tempFlashMACStruct.mac;
  /* pHash is changed on cli.cpp */

  memset(flashAccessBuffer, 0, sizeof(flashAccessBuffer));
  memcpy(flashAccessBuffer, &tempFlashMACStruct, FlashMACStructSize);
  flashWriteBlock_with_erase(flashAccessBuffer, flashMACAddr, FlashMACStructSize, FLASH_LORA_START_PAGE, FLASH_LORA_END_PAGE);
}