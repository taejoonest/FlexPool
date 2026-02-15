/*
 * RAW RECEIVE TEST - Upload this to the Pump ESP32
 * 
 * This does NOTHING except print every byte received on Serial2.
 * No protocol parsing, no delays, no processing.
 * Just raw UART → Serial Monitor.
 * 
 * Wiring: same as Pump.ino
 *   GPIO 19 ← MAX485 RO
 *   GPIO 18 → MAX485 DI  (unused in this test)
 *   GPIO 4  → MAX485 DE & RE (held LOW = receive mode)
 */

#define RS485_RX_PIN    19
#define RS485_TX_PIN    18
#define RS485_DE_RE_PIN 4

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW);  // Receive mode always
  
  Serial2.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  
  Serial.println("\n=== RAW RECEIVE TEST ===");
  Serial.println("Every byte on GPIO 19 will be printed.");
  Serial.println("Send command 5 from Controller and watch...\n");
}

void loop() {
  if (Serial2.available()) {
    unsigned long t = millis();
    Serial.printf("[%lu ms] Bytes: ", t);
    
    // Read all currently available bytes
    while (Serial2.available()) {
      uint8_t b = Serial2.read();
      Serial.printf("%02X ", b);
    }
    Serial.println();
  }
}
