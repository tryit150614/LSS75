#include "eeprom.h"
#include "usart.h"
#include "stdio.h"

extern I2C_HandleTypeDef hi2c4;
EEPROMFlag eepromFlag;

void MX_I2C4_Init(void)
{

  hi2c4.Instance = I2C4;
  hi2c4.Init.Timing = 0x00303D5B;
  hi2c4.Init.OwnAddress1 = 0;
  hi2c4.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c4.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c4.Init.OwnAddress2 = 0;
  hi2c4.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c4.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c4.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c4) != HAL_OK)
  {
    writeString(DEBUG_USART, "I2C fail 1\r\n");
    Error_Handler();
  }
  /** Configure Analogue filter 
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c4, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    writeString(DEBUG_USART, "I2C fail 2\r\n");
    Error_Handler();
  }
  /** Configure Digital filter 
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c4, 0) != HAL_OK)
  {
    writeString(DEBUG_USART, "I2C fail 3\r\n");
    Error_Handler();
  }

}

bool parSave(uint8_t* par, uint16_t addr, uint16_t len) {
    HAL_StatusTypeDef sts;
    bool              rtn = true;

    EEPROM_DisableProtect();

    if(len <= pageBytes) {
        sts = HAL_I2C_Mem_Write(&hi2c4, (uint16_t)eprAddress, addr, I2C_MEMADD_SIZE_16BIT, par, len, 1000);
        if(sts == HAL_OK) {
            HAL_Delay(5);
            rtn = true;
        }
    }
    /* Data will cross the page. */
    else {
        uint16_t pageTotal  = len / pageBytes;
        uint16_t curAddr = addr;
        for(int i = 0; i < pageTotal; i++) {
            sts = HAL_I2C_Mem_Write(&hi2c4, (uint16_t)eprAddress, curAddr, I2C_MEMADD_SIZE_16BIT, par + pageBytes * i, pageBytes, 1000);
            HAL_Delay(5);
            if(sts == HAL_OK) {
                curAddr += pageBytes;
            }
            else {
                rtn = false;
                i = pageTotal;
            }
        }
    }

    EEPROM_EnableProtect();

    return rtn;
}

bool parLoad(uint8_t* buf, uint16_t addr, uint16_t len) {

    uint8_t            emptyBuf[2+1] = {0};
    HAL_StatusTypeDef  sts             = HAL_I2C_Master_Transmit(&hi2c4, eprAddress, emptyBuf, 2, 1000);
    bool               rtn             = false;

    sts = HAL_I2C_Mem_Read(&hi2c4, (uint16_t)(eprAddress + 1), addr, I2C_MEMADD_SIZE_16BIT, buf, len, 1000);
    if(sts == HAL_OK) {
        HAL_Delay(5);
        rtn = true;
    }

    return rtn;
}

void changeSaveAddress(uint16_t newAddress) {
    uint16_t dataCount = newAddress / payloadLen;
    if(dataCount >= (MAX_EEPROM_SIZE / payloadLen)) dataCount = dataFirstAddress / payloadLen;
    // uint8_t temp[4+1] = "0001";
    uint8_t temp[4+1] = {0};
    if (dataCount < 250) 
    {
        temp[0] = (uint8_t)dataCount;
        temp[1] = 0;
        temp[2] = 0;
        temp[3] = 0;
    }
    else if(dataCount >=250 && dataCount < 500) {
        temp[0] = 250;
        temp[1] = (uint8_t)dataCount - 250;
        temp[2] = 0;
        temp[3] = 0;
    }
    else if(dataCount >=500 && dataCount < 750) {
        temp[0] = 250;
        temp[1] = 250;
        temp[2] = (uint8_t)dataCount - 500;
        temp[3] = 0;
    }
    else if(dataCount >=750 && dataCount < 1000) {
        temp[0] = 250;
        temp[1] = 250;
        temp[2] = 250;
        temp[3] = (uint8_t)dataCount - 750;
    }
    else if(dataCount == 1000) {
        temp[0] = 250;
        temp[1] = 250;
        temp[2] = 250;
        temp[3] = 250;
    }
    parSave(temp, 0, 4);
    eepromFlag.nextSaveAddress = (uint16_t)((temp[0] + temp[1] + temp[2] + temp[3]) * payloadLen);
}

void changeSendAddress(uint16_t newAddress) {
    uint16_t dataCount = newAddress / payloadLen;
    if(dataCount >= (MAX_EEPROM_SIZE / payloadLen)) dataCount = dataFirstAddress / payloadLen;
    uint8_t temp[4+1] = {0};
    if (dataCount < 250) 
    {
        temp[0] = (uint8_t)dataCount;
        temp[1] = 0;
        temp[2] = 0;
        temp[3] = 0;
    }
    else if(dataCount >=250 && dataCount < 500) {
        temp[0] = 250;
        temp[1] = (uint8_t)dataCount - 250;
        temp[2] = 0;
        temp[3] = 0;
    }
    else if(dataCount >=500 && dataCount < 750) {
        temp[0] = 250;
        temp[1] = 250;
        temp[2] = (uint8_t)dataCount - 500;
        temp[3] = 0;
    }
    else if(dataCount >=750 && dataCount < 1000) {
        temp[0] = 250;
        temp[1] = 250;
        temp[2] = 250;
        temp[3] = (uint8_t)dataCount - 750;
    }
    else if(dataCount == 1000) {
        temp[0] = 250;
        temp[1] = 250;
        temp[2] = 250;
        temp[3] = 250;
    }
    parSave(temp, 4, 4);
    eepromFlag.nextSendAddress = (uint16_t)((temp[0] + temp[1] + temp[2] + temp[3]) * payloadLen);
}

void clearAllDataInEEPROM() {
    uint8_t nonData[MAX_EEPROM_SIZE + 1] = {0};
    for(int i = 0; i < MAX_EEPROM_SIZE; i++)
        nonData[i] = 0xFF;
    
    bool rtn = parSave(nonData, 0, MAX_EEPROM_SIZE);
    if(rtn) {
        writeString(DEBUG_USART, "Success to clear all data in EEPROM.\r\n");
        changeSaveAddress(dataFirstAddress);
        changeSendAddress(dataFirstAddress);
    }
    else    writeString(DEBUG_USART, "Fail to clear all data in EEPROM.\r\n");
}

int eeprom_load_eepromFlag() {
    uint8_t temp[8+1] = {0};
    bool loadResult = parLoad(temp, (uint16_t)0, (uint16_t)9);
    if(loadResult) {
      if(temp[8] == 1) { // Need to clear eeprom data
        DEBUG("Clear eeprom...\r\n");
        clearAllDataInEEPROM();
        DEBUG("Done\r\n");

        uint8_t zero[1] = {0};
        parSave(zero, 8, 1);
        parLoad(temp, (uint16_t)0, (uint16_t)9);
      }
      eepromFlag.nextSaveAddress = (uint16_t)((temp[0] + temp[1] + temp[2] + temp[3]) * payloadLen);
      eepromFlag.nextSendAddress = (uint16_t)((temp[4] + temp[5] + temp[6] + temp[7]) * payloadLen);
      return 1;
    }
    else {
      return 0;
    }
}

int eeprom_saveData_and_jumpNextPos(uint8_t * data) {
    bool res = parSave(data, eepromFlag.nextSaveAddress, payloadLen);
    if(res) changeSaveAddress(eepromFlag.nextSaveAddress + payloadLen);

    return res ? 1 : 0;
}