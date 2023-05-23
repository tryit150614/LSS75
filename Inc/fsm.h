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

void fsm_init();

/* each function is a different state of the FSM */
funcptr fsm_idle();
funcptr fsm_getRS485Data();
funcptr fsm_getADCValue();
funcptr fsm_checkDataInEEPROM();
funcptr fsm_sleep();

funcptr nb_idle();
funcptr nb_writeNewDataInBuffer();
funcptr nb_waitIP();
funcptr nb_checkEEPROM();
funcptr nb_initHttp();
funcptr nb_getSignalPower();
funcptr nb_writeDataInNBIOT();
funcptr nb_getResponse();
funcptr nb_sendData();

funcptr fsm_checkModule();
funcptr catM_Connect_And_Send();
funcptr catM_getResponse();

void TaskADC(void * parameter);

#endif /* FSM_H_ */
