#ifndef RS232_H
#define RS232_H

#include "../const/protocol.h"

void setupRs232();
void loopRs232(uint32_t now);
void onWsEventRs232(void* arg, uint8_t* data, size_t len, uint32_t clientId, uint8_t task);

extern Rs232 rs232;

#endif
