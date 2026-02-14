/*
 * Pump Controller ESP32
 * 
 * This ESP32 acts as a bridge between:
 * - Computer (nodejs-poolController) via USB Serial
 * - Pump (or Pump Simulator) via RS-485
 * 
 * Flow:
 *   1. Receives hex commands from USB Serial (from computer)
 *   2. Converts hex string to bytes
 *   3. Sends bytes to RS-485 bus (to pump)
 *   4. Receives responses from RS-485 (from pump)
 *   5. Forwards responses back to USB Serial (to computer)
 * 
 * Hardware:
 *   - USB-C: Connected to computer
 *   - GPIO 4:  RS-485 DE/RE control pin (to MAX485)
 *   - GPIO 17: RS-485 TX pin (to MAX485 DI)
 *   - GPIO 16: RS-485 RX pin (from MAX485 RO)
 */

#include "RS485Simple.h"

// ============================================
// RS-485 Configuration
// ============================================
#define RS485_DE_RE_PIN 4    // Pin to control transmit/receive mode on MAX485
#define RS485_TX_PIN 17      // ESP32 pin for transmitting data
#define RS485_RX_PIN 16      // ESP32 pin for receiving data
#define RS485_BAUD_RATE 9600 // Communication speed (matches pump)

// Create RS-485 object using Serial2 (ESP32's second hardware serial port)
RS485Simple rs485(Serial2, RS485_DE_RE_PIN, RS485_TX_PIN, RS485_RX_PIN);

// ============================================
// Data Buffers
// ============================================
uint8_t rxBuffer[256];  // Buffer for receiving data from pump
uint8_t txBuffer[256];  // Buffer for sending data to pump

// ============================================
// Setup Function - Runs Once at Startup
// ============================================
void setup() {
  // Initialize USB Serial communication (to computer)
  // This is what nodejs-poolController will use to send commands
  Serial.begin(115200);
  delay(1000);  // Wait for serial port to initialize
  
  // Print startup message
  Serial.println("\n=================================");
  Serial.println("Pump Controller ESP32");
  Serial.println("Ready to receive commands");
  Serial.println("=================================\n");
  
  // Initialize RS-485 communication (to pump)
  rs485.begin(RS485_BAUD_RATE);
  Serial.printf("RS-485 initialized at %d baud\n", RS485_BAUD_RATE);
  Serial.println("Waiting for commands from computer...\n");
}

// ============================================
// Loop Function - Runs Continuously
// ============================================
void loop() {
  // ==========================================
  // PART 1: Check for Commands from Computer
  // ==========================================
  // IMPORTANT: nodejs-poolController sends RAW BINARY BYTES, not hex strings!
  // For manual testing with Serial Monitor, you can send hex strings
  // But for nodejs-poolController, it sends raw bytes directly
  
  if (Serial.available()) {
    // Check if it's a hex string (for manual testing) or binary (from nodejs-poolController)
    // Hex strings typically start with readable characters, binary is... binary
    
    // Try to read as binary first (for nodejs-poolController)
    size_t len = Serial.readBytes(txBuffer, 256);
    
    if (len > 0) {
      // Send the bytes directly to pump via RS-485
      rs485.write(txBuffer, len);
      
      // Print confirmation (for debugging)
      Serial.printf("Sent to pump (%d bytes): ", len);
      for (size_t i = 0; i < len; i++) {
        Serial.printf("%02X ", txBuffer[i]);  // Print as hex for readability
      }
      Serial.println();
    }
  }
  
  // ==========================================
  // PART 2: Check for Responses from Pump
  // ==========================================
  // Pump sends responses back via RS-485 as raw bytes
  if (rs485.available()) {
    // Read response bytes from RS-485
    size_t len = rs485.readBytes(rxBuffer, 256);
    
    if (len > 0) {
      // Forward response to computer via USB Serial
      // Send as RAW BYTES (not hex string) for nodejs-poolController
      Serial.write(rxBuffer, len);  // Use write() not print() for binary
      
      // Also print as hex for debugging
      Serial.print(" [Pump response: ");
      for (size_t i = 0; i < len; i++) {
        Serial.printf("%02X ", rxBuffer[i]);
      }
      Serial.println("]");
    }
  }
  
  // Small delay to prevent CPU from running at 100%
  delay(10);
}

// ============================================
// How It Works - Step by Step
// ============================================
/*
 * EXAMPLE: Setting pump speed to 564 RPM
 * 
 * 1. nodejs-poolController sends: "1001023456"
 *    (This is a hex string representing bytes)
 * 
 * 2. PumpController.ino receives it via Serial
 * 
 * 3. Converts hex string to bytes:
 *    "10" -> 0x10 (16)
 *    "01" -> 0x01 (1)
 *    "02" -> 0x02 (2)
 *    "34" -> 0x34 (52)
 *    "56" -> 0x56 (86)
 *    Result: [0x10, 0x01, 0x02, 0x34, 0x56]
 * 
 * 4. Sends bytes to pump via RS-485:
 *    rs485.write([0x10, 0x01, 0x02, 0x34, 0x56], 5)
 * 
 * 5. Pump receives command and responds
 * 
 * 6. PumpController receives response via RS-485
 * 
 * 7. Converts response bytes to hex string and sends to Serial
 *    Example: "10820234560100"
 * 
 * 8. nodejs-poolController receives response
 */

// ============================================
// Testing Without nodejs-poolController
// ============================================
/*
 * You can test this manually using Serial Monitor:
 * 
 * 1. Open Serial Monitor (115200 baud)
 * 2. Make sure "Newline" is selected
 * 3. Type: 1001023456
 * 4. Press Enter
 * 5. You should see: "Sent to pump: 10 01 02 34 56"
 * 6. If pump responds, you'll see: "Pump response: ..."
 */
