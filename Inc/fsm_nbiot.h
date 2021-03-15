#ifndef _FSM_NBIOT_H
#define _FSM_NBIOT_H

// #include "payloadHandlers.h"

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

typedef int (*NB)();
typedef NB (*StateNB)();

struct FSMAttributes_NB {

  StateNB state;

  char payload   [240+1];
  char payloadOut[240+1];
  // struct decodedPayloadStruct decodedPayload;
};

void nb_init();

/* each function is a different state of the FSM */
NB nb_idle();
NB nb_waitIP();
NB nb_initHttp();
NB nb_ConnectSocket();
NB nb_sendData();
NB nb_sleep();

#endif /* FSM_NBIOT_H_ */
