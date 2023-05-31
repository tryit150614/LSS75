#include "Cli.h"
#include "main.h"
#include "serialHandlers.h"
#include "usart.h"
#include "nodeManager.h"
#include "cmsis_os.h"

extern struct inputString inputStringUart4;
extern volatile ringBufferStruct uart4Buf;

extern char pHash[9];

void TaskCLI(void *pvParameters) {
  for (;;) {
      parseStringInBuffer(&inputStringUart4, &uart4Buf);
      if (inputStringUart4.ready == 1) {
          inputStringUart4.ready = 0;
          /* $00011FWzw5ad6 */
          if(inputStringUart4.string[0] == '$') {
              char macStr[6] = {0};
              memcpy(macStr, inputStringUart4.string + 1, 5);
              memcpy(pHash, inputStringUart4.string + 6, 8);
              uint32_t mac = strtol(macStr, NULL, 10);
              DEBUG("******* Change MAC to ");
              writeUint32(DEBUG_USART, mac);
              DEBUG(" *******\r\n");
              DEBUG("******* Change hash to ");
              DEBUG(pHash);
              DEBUG(" *******\r\n");

              nodeManager_modifyMAC(mac);
              nodeManager_modifyHash(pHash);
          }
      }

      vTaskDelay(pdMS_TO_TICKS(10));
  }
}