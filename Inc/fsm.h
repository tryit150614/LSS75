#ifndef _FSM_H
#define _FSM_H

// #include "payloadHandlers.h"

typedef int (*funcptr)();
typedef funcptr (*StateFunc)();

struct FSMAttributes {

  StateFunc state;

  char payload   [240+1];
  char payloadOut[240+1];
  // struct decodedPayloadStruct decodedPayload;
};

void TaskFSM(void *pvParameters);
void fsm_init();

/* each function is a different state of the FSM */
funcptr fsm_idle();
funcptr fsm_getRS485Data();
funcptr fsm_initModule();
funcptr fsm_getRSSI();
funcptr fsm_prepareNewPayload();
funcptr fsm_checkEEPROM();
funcptr fsm_sendData();
funcptr fsm_getResponse();
funcptr fsm_sleep();

#endif /* FSM_H_ */
