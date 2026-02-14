/*
 * Serial Command Handler Header
 * 
 * Processes commands from Serial interface
 */

#ifndef SERIAL_COMMAND_HANDLER_H
#define SERIAL_COMMAND_HANDLER_H

#include <Arduino.h>
#include "RS485.h"
#include "Statistics.h"
#include "config.h"  // All headers in same include directory

class SerialCommandHandler {
public:
    SerialCommandHandler(RS485& rs485, Statistics& stats);
    
    void processCommand(const String& command);

private:
    RS485& rs485_;
    Statistics& stats_;
    
    void handleSendCommand(const String& command);
    void printHelp();
};

#endif // SERIAL_COMMAND_HANDLER_H
