#ifndef _SARA_R410M
#define _SARA_R410M

#include "stdint.h"

int sara_ready();
int sara_writeInFileSystem(char * payload, uint32_t len);
int sara_setHttp();
int sara_getResponse(int sec, char* target);
int sara_getResponse_safety(int sec, char* target);
int sara_http_send();

// void sendCmd(char * str);

#endif /* _SARA_R410M */
