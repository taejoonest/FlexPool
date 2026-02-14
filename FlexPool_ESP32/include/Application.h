/*
 * Application Class Header
 * 
 * Main application logic for FlexPool ESP32 RS-485 Interface
 */

#ifndef APPLICATION_H
#define APPLICATION_H

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include "RS485.h"
#include "Statistics.h"
#include "SerialCommandHandler.h"
#include "config.h"  // All headers in same include directory

class Application {
public:
    Application();
    
    bool initialize();
    void run();
    
    void processReceivedPacket(const uint8_t* data, size_t length);
    void sendRS485Data(const uint8_t* data, size_t length);

private:
    RS485 rs485_;
    Statistics statistics_;
    SerialCommandHandler command_handler_;
    
    uint8_t rx_buffer_[RS485_BUFFER_SIZE];
    
    unsigned long last_activity_;
    unsigned long last_stats_print_;
    
    void printHexData(const char* prefix, const uint8_t* data, size_t length);
    
    #ifdef ENABLE_WIFI
    void initWiFi();
    void forwardToNetwork(const uint8_t* data, size_t length);
    #endif
};

#endif // APPLICATION_H
