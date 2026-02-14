/*
 * RS-485 Library Implementation for ESP32
 * 
 * Handles RS-485 communication with proper DE/RE control
 */

#include "../include/RS485.h"
#include <Arduino.h>

RS485::RS485(HardwareSerial& serialPort, int de_re, int tx, int rx) 
    : serial_(&serialPort), de_re_pin_(de_re), tx_pin_(tx), rx_pin_(rx), initialized_(false) {
}

RS485::~RS485() {
    // Cleanup if needed
}

bool RS485::begin(unsigned long baudRate) {
    // Configure DE/RE pin
    pinMode(de_re_pin_, OUTPUT);
    setReceiveMode(); // Start in receive mode
    
    // Initialize serial port
    serial_->begin(baudRate, SERIAL_8N1, rx_pin_, tx_pin_);
    
    // Wait for serial to be ready
    delay(100);
    
    initialized_ = true;
    return true;
}

void RS485::setTransmitMode() {
    digitalWrite(de_re_pin_, HIGH);
    delayMicroseconds(RS485_PRE_TX_DELAY_US);
}

void RS485::setReceiveMode() {
    delayMicroseconds(RS485_POST_TX_DELAY_US);
    digitalWrite(de_re_pin_, LOW);
}

int RS485::available() const {
    if (!initialized_) return 0;
    return serial_->available();
}

size_t RS485::readBytes(uint8_t* buffer, size_t length) {
    if (!initialized_ || buffer == nullptr) return 0;
    
    size_t bytesRead = 0;
    unsigned long startTime = millis();
    
    // Read with timeout
    while (bytesRead < length && (millis() - startTime < RS485_RX_TIMEOUT_MS)) {
        if (serial_->available()) {
            buffer[bytesRead++] = serial_->read();
            startTime = millis(); // Reset timeout on each byte
        }
        delayMicroseconds(100);
    }
    
    return bytesRead;
}

size_t RS485::write(const uint8_t* data, size_t length) {
    if (!initialized_ || data == nullptr) return 0;
    
    setTransmitMode();
    
    size_t bytesWritten = serial_->write(data, length);
    serial_->flush(); // Wait for transmission to complete
    
    setReceiveMode();
    
    return bytesWritten;
}

size_t RS485::write(uint8_t byte) {
    if (!initialized_) return 0;
    
    setTransmitMode();
    
    size_t bytesWritten = serial_->write(byte);
    serial_->flush();
    
    setReceiveMode();
    
    return bytesWritten;
}

void RS485::flush() {
    if (initialized_) {
        serial_->flush();
    }
}

int RS485::peek() const {
    if (!initialized_) return -1;
    return serial_->peek();
}

int RS485::read() {
    if (!initialized_) return -1;
    return serial_->read();
}

bool RS485::isInitialized() const {
    return initialized_;
}
