#ifndef _EXS_62
#define _EXS_62

int exs62_ready();
int exs62_switchOff();
int exs62_setHttp();
int exs62_connectNetwork();
int exs62_getResponse(int sec, char* target);
int exs62_preparePayload_Connect(uint32_t len);
int exs62_closeConnetion();
int exs62_http_send(char *payload, uint32_t payloadlen);
void exs62_getRSSI(char * out);

void sendCmd(char * str);

#endif /* _EXS_62 */
