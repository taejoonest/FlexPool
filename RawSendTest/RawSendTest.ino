/*
 * RAW SEND TEST - Upload this to the Controller ESP32
 * 
 * Sends a known 11-byte test packet every 5 seconds automatically.
 * No Serial Monitor input needed — just power it on.
 * 
 * Wiring: same as Controller.ino
 *   GPIO 18 → MAX485 DI
 *   GPIO 19 ← MAX485 RO  (unused in this test)
 *   GPIO 4  → MAX485 DE & RE (tie together)
 */

#define RS485_TX_PIN    18
#define RS485_RX_PIN    19
#define RS485_DE_RE_PIN 4

// The exact packet for command 5 (status query):
// FF 00 FF A5 00 60 20 07 00 01 2C
uint8_t testPacket[] = {0xFF, 0x00, 0xFF, 0xA5, 0x00, 0x60, 0x20, 0x07, 0x00, 0x01, 0x2C};
const size_t packetLen = sizeof(testPacket);

unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW);
  
  Serial2.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  
  Serial.println("\n=== RAW SEND TEST ===");
  Serial.println("Sending test packet every 5 seconds automatically.");
  Serial.println("Expected: FF 00 FF A5 00 60 20 07 00 01 2C\n");
}

void loop() {
  if (millis() - lastSend >= 5000) {
    lastSend = millis();
    
    Serial.print("TX: ");
    for (size_t i = 0; i < packetLen; i++) {
      Serial.printf("%02X ", testPacket[i]);
    }
    Serial.println();
    
    // Clear stale RX
    while (Serial2.available()) Serial2.read();
    
    // Enable transmitter, minimal setup time
    digitalWrite(RS485_DE_RE_PIN, HIGH);
    delayMicroseconds(500);
    
    // Send all bytes
    Serial2.write(testPacket, packetLen);
    Serial2.flush();
    
    // Wait for all bytes to physically leave (11 bytes × ~1.1ms + 5ms margin)
    delay(17);
    
    // Back to receive mode
    digitalWrite(RS485_DE_RE_PIN, LOW);
    
    Serial.println("Sent! (next in 5 seconds)");
  }
}
