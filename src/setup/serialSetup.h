#ifndef SERIAL_SETUP_H
#define SERIAL_SETUP_H

#include <Arduino.h>

void loopSerialSetup();
void handleVuespCommand(char* command, Print& out);

#endif
