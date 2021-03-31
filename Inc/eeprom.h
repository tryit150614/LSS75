#ifndef __EPPROM_H
#define __EPPROM_H

#ifdef __cplusplus
extern "C" {
#endif

#define eprAddress       0xA0
#define flagAddress      0
#define dataFirstAddress 384
#define allPages         512   /* All pages */
#define MAX_EEPROM_SIZE  64000 /* All bytes */
#define pageBytes        128   /* Bytes of every page */
#define payloadLen       384  

#include <stdbool.h>
#include <stdint.h>
#include "main.h"

typedef struct EEPROMFlagStruct {
    uint16_t nextSaveAddress;
    uint16_t nextSendAddress;
}EEPROMFlag;

void     MX_I2C4_Init(void);

bool     parSave(uint8_t* par, uint16_t addr, uint16_t len);
bool     parLoad(uint8_t* buf, uint16_t addr, uint16_t len);
void     changeSaveAddress(uint16_t newAddress);
void     changeSendAddress(uint16_t newAddress);
void     clearAllDataInEEPROM();

// uint32_t rtvData(uint8_t* dat, uint8_t pnt, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* __EPPROM_H */