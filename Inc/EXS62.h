#ifndef _EXS_62
#define _EXS_62

int exs62_ready();
int exs62_setHttp();
int exs62_getResponse(int sec, char* target);
int exs62_getResponse_safety(int sec, char* target);

void sendCmd(char * str);

#endif /* _EXS_62 */
