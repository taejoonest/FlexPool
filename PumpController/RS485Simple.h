/*
 * Simple RS-485 Library for ESP32
 * Arduino IDE compatible - single header file
 */

#ifndef RS485SIMPLE_H
#define RS485SIMPLE_H

#include <HardwareSerial.h>
#include <Arduino.h>

class RS485Simple {
private:
  HardwareSerial* serial;
  int de_re_pin;
  int tx_pin;
  int rx_pin;
  
  void setTransmitMode() {
    digitalWrite(de_re_pin, HIGH);
    delayMicroseconds(10);
  }
  
  void setReceiveMode() {
    delayMicroseconds(10);
    digitalWrite(de_re_pin, LOW);
  }
  
public:
  RS485Simple(HardwareSerial& serialPort, int de_re, int tx, int rx) 
    : serial(&serialPort), de_re_pin(de_re), tx_pin(tx), rx_pin(rx) {
  }
  
  bool begin(unsigned long baudRate) {
    pinMode(de_re_pin, OUTPUT);
    setReceiveMode();
    serial->begin(baudRate, SERIAL_8N1, rx_pin, tx_pin);
    delay(100);
    return true;
  }
  
  int available() {
    return serial->available();
  }
  
  size_t readBytes(uint8_t* buffer, size_t length) {
    size_t bytesRead = 0;
    unsigned long startTime = millis();
    
    while (bytesRead < length && (millis() - startTime < 100)) {
      if (serial->available()) {
        buffer[bytesRead++] = serial->read();
        startTime = millis();
      }
      delayMicroseconds(100);
    }
    
    return bytesRead;
  }
  
  size_t write(const uint8_t* data, size_t length) {
    setTransmitMode();
    size_t bytesWritten = serial->write(data, length);
    serial->flush();
    setReceiveMode();
    return bytesWritten;
  }
};

#endif
