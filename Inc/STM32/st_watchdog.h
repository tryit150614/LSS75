#ifndef WATCHDOG_H_
#define WATCHDOG_H_

class Watchdog {
public:
  void enable();
  void reload();
};

// Task declarations
#ifdef __cplusplus
extern "C" {
#endif

void vWatchdog(void * pvParameters);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* WATCHDOG_H_ */
