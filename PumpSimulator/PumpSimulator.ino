/*
 * Pump Simulator ESP32
 * 
 * Simulates a Pentair pump on RS-485 bus
 * Receives speed commands and responds with status
 * 
 * Connection:
 * - RS-485 to Controller ESP32
 * - No USB needed (but can use for debugging)
 */

#include "RS485Simple.h"
#include "PumpSimulator.h"

// RS-485 Configuration
#define RS485_DE_RE_PIN 4
#define RS485_TX_PIN 17
#define RS485_RX_PIN 16
#define RS485_BAUD_RATE 9600

RS485Simple rs485(Serial2, RS485_DE_RE_PIN, RS485_TX_PIN, RS485_RX_PIN);
PumpSimulator pump;

// Buffer for data
uint8_t rxBuffer[256];
uint8_t txBuffer[256];

void setup() {
  // USB Serial for debugging (optional)
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=================================");
  Serial.println("Pump Simulator ESP32");
  Serial.println("Simulating Pentair Pump");
  Serial.println("=================================\n");
  
  // Initialize RS-485
  rs485.begin(RS485_BAUD_RATE);
  Serial.printf("RS-485 initialized at %d baud\n", RS485_BAUD_RATE);
  
  // Initialize pump simulator
  pump.begin();
  Serial.println("Pump simulator ready");
  Serial.println("Current speed: 0 RPM (stopped)\n");
}

void loop() {
  // Check for commands from RS-485 (from controller)
  if (rs485.available()) {
    size_t len = rs485.readBytes(rxBuffer, 256);
    if (len > 0) {
      Serial.print("Received command: ");
      for (size_t i = 0; i < len; i++) {
        Serial.printf("%02X ", rxBuffer[i]);
      }
      Serial.println();
      
      // Process command and get response
      size_t responseLen = pump.processCommand(rxBuffer, len, txBuffer);
      
      if (responseLen > 0) {
        // Send response back
        rs485.write(txBuffer, responseLen);
        
        Serial.print("Sent response: ");
        for (size_t i = 0; i < responseLen; i++) {
          Serial.printf("%02X ", txBuffer[i]);
        }
        Serial.println();
      }
    }
  }
  
  // Update pump status periodically
  pump.update();
  
  delay(10);
}
