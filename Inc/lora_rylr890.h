#ifndef RYLR890_H
#define RYLR890_H

#define RYLR890_PAYLOAD_MAX_LEN 240

int lora_setParams();
int lora_sendData(char * DeviceNetworkID, char * data);
int lora_getPacket(char * in, char * payload);

#endif /* RYLR890_H */
