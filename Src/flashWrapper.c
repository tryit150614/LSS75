
#include <stdint.h>
#include "flashWrapper.h"
#include "stm32l4xx_hal_flash_ex.h"
#include "stm32l4xx_hal_flash.h"
#include "usart.h"
#include "nodeManager.h"
#include "main.h"

extern const unsigned long ResendStructSize;
volatile uint32_t flashResendAddr = FLASH_RESEND_START_ADDR;
volatile uint32_t flashMACAddr = FLASH_MAC_START_ADDR;

extern struct ResendStruct flashResend;
extern volatile uint32_t pResendTime;
extern volatile uint32_t pMAC;
extern char pHash[9];

/* Private functions */

uint32_t PageError = 0;

uint32_t flashWriteBlock_first(uint64_t * src, uint32_t destAddr, unsigned int numbBytes) {

  HAL_FLASH_Unlock();

  unsigned int loopIter = numbBytes / 8;
  uint32_t addr = (uint32_t) destAddr;
  for(int i = 0; i < loopIter; i++) {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, src[i]);
    addr = addr + 8;
  }

  HAL_FLASH_Lock();

  return 0;
}

uint32_t flashWriteBlock_with_erase(uint64_t * src, uint32_t destAddr, unsigned int numbBytes, uint32_t start_page, uint32_t end_page) {

  HAL_FLASH_Unlock();

  HAL_StatusTypeDef ret = flashErasePages(FLASH_TYPEERASE_PAGES, start_page, end_page);
  if (ret != HAL_OK) {
    writeString(DEBUG_USART, "Flash erase fail.\r\n");
    HAL_Delay(5);
    ret = flashErasePages(FLASH_TYPEERASE_PAGES, start_page, end_page);
    if (ret != HAL_OK) writeString(DEBUG_USART, "Flash erase fail again.\r\n");
  }

  unsigned int loopIter = numbBytes / 8;
  uint32_t addr = (uint32_t) destAddr;
  for(int i = 0; i < loopIter; i++) {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, src[i]);
    addr = addr + 8;
  }

  HAL_FLASH_Lock();

  return 0;
}

uint32_t flashWriteBlock(uint64_t * src) {
  static uint8_t writeCount = 0;

  HAL_FLASH_Unlock();

  unsigned int loopIter = ResendStructSize / 8;
  uint32_t addr = flashResendAddr;
  
  for(int i = 1; i <= loopIter; i++) {
    /* Check next position is not limit.*/
    if(addr + ResendStructSize >= FLASH_RESEND_END_ADDR) {
      writeString(DEBUG_USART, "Over limit, erase all page.\r\n");
      HAL_StatusTypeDef ret = flashErasePages(FLASH_TYPEERASE_PAGES, FLASH_RESEND_START_PAGE, FLASH_RESEND_END_PAGE);
      if (ret != HAL_OK) {
        writeString(DEBUG_USART, "Flash erase fail.\r\n");
      }

      addr = FLASH_RESEND_START_ADDR;
      i = 0;
    }
    else {
      /* clear old data */
      HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, 0);
      /* write new data */
      HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + ResendStructSize, src[i - 1]);

      /* Check the written value */
      if(*(uint64_t*)(addr + ResendStructSize) != src[i - 1])
      {
          /* Write data again from flashTitle */
          writeString(DEBUG_USART, "flash write fail, rewrite from flashTitle.\r\n");
          writeCount++;
          if(writeCount >= 10) i = 10;
          else                 i = 0;
      }
      
      addr = addr + 8;
    }
  }

  // flashResendAddr   = addr;
  // if(writeCount   < 10) {
  //   writeCount    = 0;
  //   pStart        = flashRead(flashResendAddr);
  //   pResendTime   = flashRead(flashResendAddr + 8);
  // }
  // else {
  //   writeCount = 0;
  // }
  

  HAL_FLASH_Lock();

  return 0;
}

uint32_t flashReadBlock(uint32_t flashAddress, uint64_t * dest, unsigned int numbBytes) {
  unsigned int loopIter = numbBytes / 8;
  uint32_t addr = flashAddress;
  for(int i = 0; i < loopIter; i++) {
    dest[i] = flashRead(addr);
    addr = addr + 8;
  }
  return 0;
}

HAL_StatusTypeDef flashErasePages(uint8_t typeErase, uint32_t startPage, uint32_t endPage ) {

  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

  static FLASH_EraseInitTypeDef EraseInitStruct;
  EraseInitStruct.Banks       = FLASH_BANK_1;
  EraseInitStruct.TypeErase   = typeErase;
  EraseInitStruct.Page        = startPage;
  EraseInitStruct.NbPages     = endPage - startPage;
  return (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError));
}

uint64_t flashRead(uint32_t flashAddress) {
    __IO uint64_t flashData;

    flashData = * (__IO uint64_t *) flashAddress;

    return flashData;
}

void findFlashPosition() {

  pResendTime = flashRead(FLASH_RESEND_START_ADDR);
  pMAC = flashRead(FLASH_MAC_START_ADDR);
  flashReadBlock(FLASH_HASH_START_ADDR, (uint64_t *)pHash, ResendStructSize);
}

void resetFlashParameter() {
  pResendTime  = 600;        // 10 mins
}