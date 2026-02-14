/*
 * Pump Controller ESP32 - Binary Mode
 * 
 * CORRECTED VERSION for nodejs-poolController
 * 
 * nodejs-poolController sends RAW BINARY BYTES, not hex strings!
 * 
 * This version handles binary communication properly.
 * 
 * Flow:
 *   1. Receives RAW BYTES from USB Serial (from nodejs-poolController)
 *   2. Forwards RAW BYTES to RS-485 bus (to pump)
 *   3. Receives RAW BYTES from RS-485 (from pump)
 *   4. Forwards RAW BYTES back to USB Serial (to nodejs-poolController)
 * 
 * Hardware:
 *   - USB-C: Connected to computer (nodejs-poolController)
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

// USB Serial baud rate (for nodejs-poolController)
// nodejs-poolController typically uses 9600 or 115200
#define USB_SERIAL_BAUD 115200

// Create RS-485 object using Serial2 (ESP32's second hardware serial port)
RS485Simple rs485(Serial2, RS485_DE_RE_PIN, RS485_TX_PIN, RS485_RX_PIN);

// ============================================
// Data Buffers
// ============================================
uint8_t rxBuffer[256];  // Buffer for receiving data from pump
uint8_t txBuffer[256];  // Buffer for sending data to pump

// Statistics
unsigned long packetsFromComputer = 0;
unsigned long packetsToPump = 0;
unsigned long packetsFromPump = 0;
unsigned long packetsToComputer = 0;

// ============================================
// Setup Function - Runs Once at Startup
// ============================================
void setup() {
  // Initialize USB Serial communication (to computer/nodejs-poolController)
  // nodejs-poolController will connect to this COM port
  Serial.begin(USB_SERIAL_BAUD);
  delay(1000);  // Wait for serial port to initialize
  
  // Print startup message
  Serial.println("\n=================================");
  Serial.println("Pump Controller ESP32");
  Serial.println("Binary Mode - Ready for nodejs-poolController");
  Serial.println("=================================\n");
  
  // Initialize RS-485 communication (to pump)
  rs485.begin(RS485_BAUD_RATE);
  Serial.printf("USB Serial: %d baud (for nodejs-poolController)\n", USB_SERIAL_BAUD);
  Serial.printf("RS-485: %d baud (for pump)\n", RS485_BAUD_RATE);
  Serial.println("\nReady. Waiting for binary data from nodejs-poolController...\n");
}

// ============================================
// Loop Function - Runs Continuously
// ============================================
void loop() {
  // ==========================================
  // PART 1: Receive RAW BYTES from Computer
  // ==========================================
  // nodejs-poolController sends RAW BINARY BYTES via USB Serial
  // NOT hex strings! Just raw bytes like [0x10, 0x01, 0x02, ...]
  if (Serial.available()) {
    // Read raw bytes directly (no string conversion)
    size_t len = Serial.readBytes(txBuffer, 256);
    
    if (len > 0) {
      packetsFromComputer++;
      
      // Send raw bytes directly to pump via RS-485
      // No conversion needed - already in binary format
      size_t sent = rs485.write(txBuffer, len);
      
      if (sent == len) {
        packetsToPump++;
        // Optional: Print for debugging (can disable in production)
        Serial.printf("[DEBUG] Forwarded %d bytes to pump\n", len);
      } else {
        Serial.printf("[ERROR] Failed to send all bytes. Sent: %d/%d\n", sent, len);
      }
    }
  }
  
  // ==========================================
  // PART 2: Receive RAW BYTES from Pump
  // ==========================================
  // Pump sends responses back via RS-485 as raw bytes
  if (rs485.available()) {
    // Read raw bytes directly from RS-485
    size_t len = rs485.readBytes(rxBuffer, 256);
    
    if (len > 0) {
      packetsFromPump++;
      
      // Forward raw bytes directly to computer via USB Serial
      // No conversion needed - send as binary
      Serial.write(rxBuffer, len);
      packetsToComputer++;
      
      // Optional: Print for debugging (can disable in production)
      Serial.printf("[DEBUG] Forwarded %d bytes to computer\n", len);
    }
  }
  
  // Print statistics every 10 seconds (optional)
  static unsigned long lastStats = 0;
  if (millis() - lastStats > 10000) {
    Serial.printf("[STATS] Computer->Pump: %lu, Pump->Computer: %lu\n", 
                   packetsToPump, packetsToComputer);
    lastStats = millis();
  }
  
  // Small delay to prevent CPU from running at 100%
  delay(1);
}

// ============================================
// How nodejs-poolController Works
// ============================================
/*
 * nodejs-poolController Configuration:
 * 
 * In config.json:
 * {
 *   "controller": {
 *     "rs485Port": "COM3",  // Your ESP32's COM port
 *     "portSettings": {
 *       "baudRate": 115200,  // Must match USB_SERIAL_BAUD above
 *       "dataBits": 8,
 *       "stopBits": 1,
 *       "parity": "none"
 *     }
 *   }
 * }
 * 
 * nodejs-poolController will:
 * 1. Open COM3 as a serial port
 * 2. Send raw binary bytes (Pentair protocol commands)
 * 3. Expect raw binary bytes back (pump responses)
 * 
 * This ESP32 code:
 * 1. Receives those raw bytes via Serial
 * 2. Forwards them to RS-485
 * 3. Receives responses from RS-485
 * 4. Forwards them back via Serial
 * 
 * NO HEX CONVERSION - just pass through binary data!
 */
