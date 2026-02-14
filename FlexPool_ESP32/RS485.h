/*
 * RS-485 Library Header for ESP32
 * 
 * Handles RS-485 communication with proper DE/RE control
 */

#ifndef RS485_H
#define RS485_H

#include <HardwareSerial.h>
#include <cstddef>
#include <cstdint>
#include "config.h"

// Forward declaration if config.h is in different location
// For PlatformIO structure, config.h should be in include/ or src/

class RS485 {
public:
    // Constructor
    RS485(HardwareSerial& serialPort, int de_re, int tx, int rx);
    
    // Destructor
    ~RS485();
    
    // Initialize RS-485
    bool begin(unsigned long baudRate);
    
    // Check if data is available
    int available() const;
    
    // Read bytes from RS-485
    size_t readBytes(uint8_t* buffer, size_t length);
    
    // Write bytes to RS-485
    size_t write(const uint8_t* data, size_t length);
    
    // Write single byte
    size_t write(uint8_t byte);
    
    // Flush serial buffer
    void flush();
    
    // Peek at next byte
    int peek() const;
    
    // Read single byte
    int read();
    
    // Check if initialized
    bool isInitialized() const;

private:
    HardwareSerial* serial_;
    int de_re_pin_;
    int tx_pin_;
    int rx_pin_;
    bool initialized_;
    
    // Switch to transmit mode
    void setTransmitMode();
    
    // Switch to receive mode
    void setReceiveMode();
};

#endif // RS485_H
