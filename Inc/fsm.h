#ifndef _FSM_H
#define _FSM_H

// #include "payloadHandlers.h"

typedef int (*funcptr)();
typedef funcptr (*StateFunc)();

#define ERROR_MSG_CGDCONT "AT+CGDCONT?\r\r\n+CGDCONT: 1,\"IPV4V6\",\"\",\"0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0\",0,0,0,0"
#define ERROR_MSG_CGDCONT_TEXT_NUM 79
#define ERROR_MSG_USOCR "AT+USOCR=6\r\r\n+CME ERROR:"
#define ERROR_MSG_USOCR_TEXT_NUM 24
#define OK_MSG_CGDCONT "AT+CGDCONT?\r\r\n+CGDCONT: 1,\"IPV4V6\",\"internet.iot\","
#define OK_MSG_CGDCONT_TEXT_NUM 50
#define OK_MSG_USOCR    "AT+USOCR=6\r\r\n+USOCR:"
#define OK_MSG_USOCR_TEXT_NUM 20
#define OK_MSG_USOCO    "\r\nOK\r\n"
#define OK_MSG_USOCO_TEXT_NUM 6

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

#endif /* FSM_H_ */
