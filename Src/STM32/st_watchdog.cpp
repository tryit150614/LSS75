#include "stm32l451xx.h"
#include "stm32l4xx_ll_iwdg.h"

#include "st_watchdog.h"

Watchdog watchdog;

void Watchdog::enable() {
  LL_IWDG_EnableWriteAccess(IWDG);
  // Set prescaler
  while(LL_IWDG_IsActiveFlag_PVU(IWDG) == 1);
  LL_IWDG_SetPrescaler(IWDG, LL_IWDG_PRESCALER_256);
  // Set reload value
  while(LL_IWDG_IsActiveFlag_RVU(IWDG) == 1);
  LL_IWDG_SetReloadCounter(IWDG, 0xFFF);

  // Wait for status register flags to be cleared
  while((LL_IWDG_IsActiveFlag_RVU(IWDG) && LL_IWDG_IsActiveFlag_PVU(IWDG) && LL_IWDG_IsActiveFlag_WVU(IWDG)) != 0);

  LL_IWDG_DisableWriteAccess(IWDG);

  LL_IWDG_ReloadCounter(IWDG);

  LL_IWDG_Enable(IWDG);

}

void Watchdog::reload() {
  LL_IWDG_ReloadCounter(IWDG);
}

// Task definitions
void vWatchdog(void * pvParameters) {
  Watchdog * watchdog = (Watchdog *) pvParameters;
  for(;;) {
    watchdog->reload();
  }

}

