/*
 * =============================================
 * CONTROLLER ESP32 - WiFi + RS-485
 * =============================================
 * 
 * Upload this to your Controller ESP32.
 * 
 * TWO ways to control the pump:
 *   1. USB Serial Monitor (type command numbers)
 *   2. WiFi Web Interface (open http://flexpool.local on your phone)
 * 
 * This implements the REAL Pentair IntelliFlo RS-485 protocol
 * with commands matching EXACTLY what nodejs-poolController sends:
 *   - github.com/tagyoureit/nodejs-poolController
 *   - Verified from: controller/nixie/pumps/Pump.ts
 * 
 * COMMAND SEQUENCE (how nodejs-poolController does it):
 *   1. Start motor      (CMD 0x06, data=0x0A)
 *   2. Set RPM directly (CMD 0x01, reg=0x02C4, data=RPM)
 *   3. Request status   (CMD 0x07)
 *   4. Set remote ctrl  (CMD 0x04, data=0xFF)
 *   ...repeats periodically to keep pump running...
 *   To stop: Set drive stop (CMD 0x06, data=0x04), then local (CMD 0x04, data=0x00)
 * 
 * Wiring:
 *   ESP32 GPIO 17 --> MAX3485 DI
 *   ESP32 GPIO 16 <-- MAX3485 RO
 *   ESP32 GPIO 4  --> MAX3485 DE & RE (tie together)
 *   ESP32 GND     --> MAX3485 GND
 *   ESP32 3.3V    --> MAX3485 VCC
 *   MAX3485 A     --> (wire to Pump ESP32's MAX3485 A)
 *   MAX3485 B     --> (wire to Pump ESP32's MAX3485 B)
 *   120 ohm resistor between A and B (termination)
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HardwareSerial.h>
#include "PentairProtocol.h"
#include "PumpStatus.h"
#include "BLESetup.h"
#include "MQTTHandler.h"
#include "WebUI.h"

// mDNS hostname - access at http://flexpool.local
const char* MDNS_HOSTNAME = "flexpool";

// =============================================
// PIN CONFIGURATION
// =============================================
#define RS485_TX_PIN    17    // ESP32 TX -> MAX3485 DI
#define RS485_RX_PIN    16    // ESP32 RX <- MAX3485 RO
#define RS485_DE_RE_PIN 4     // ESP32 -> MAX3485 DE & RE

// =============================================
// COMMUNICATION SETTINGS
// =============================================
#define RS485_BAUD  9600      // Pentair uses 9600 baud, 8N1
#define USB_BAUD    115200    // USB Serial Monitor speed

// =============================================
// OUR ADDRESSES
// =============================================
uint8_t controllerAddr = ADDR_REMOTE_CONTROLLER;  // 0x20
uint8_t pumpAddr       = ADDR_PUMP_1;              // 0x60

// =============================================
// BUFFERS
// =============================================
uint8_t txBuffer[48];   // Outgoing packet
uint8_t rxBuffer[96];   // Incoming response buffer
size_t  rxLen = 0;

// =============================================
// STATE (tracked for web UI display)
// =============================================
bool remoteControlActive = false;

// Pump status (struct defined in PumpStatus.h, shared with MQTTHandler)
PumpStatus pumpStatus;

// Web server on port 80
WebServer server(80);

// BLE provisioning handler
BLESetup bleSetup;

// MQTT handler for remote control
MQTTHandler mqtt;

// Auto-query timer
unsigned long lastAutoQuery = 0;
#define AUTO_QUERY_INTERVAL  5000  // Query pump status every 5 seconds when running

// =============================================
// SETUP
// =============================================
void setup() {
  Serial.begin(USB_BAUD);
  delay(3000);
  
  // RS-485 direction control
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW);  // Start in receive mode
  
  // RS-485 UART (Pentair: 9600 baud, 8N1)
  Serial2.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  
  printBanner();
  
  // ---- WiFi Setup via Bluetooth (no hardcoded credentials!) ----
  // Check if we have saved WiFi credentials in flash
  if (!BLESetup::hasSavedCredentials()) {
    // FIRST TIME: No credentials saved → start BLE provisioning
    Serial.println("[WiFi] No saved credentials found.");
    Serial.println("[WiFi] Starting Bluetooth setup (5 minutes)...");
    bleSetup.runProvisioning();  // Blocks until phone sends credentials, then reboots
    return;  // Never reached (ESP restarts)
  }
  
  // NORMAL BOOT: Try connecting with saved credentials
  if (!BLESetup::connectSaved()) {
    // Saved credentials didn't work → start BLE provisioning again
    Serial.println("[WiFi] Saved credentials failed. Starting Bluetooth setup...");
    bleSetup.runProvisioning();
    return;
  }
  
  // Setup mDNS (http://flexpool.local)
  if (MDNS.begin(MDNS_HOSTNAME)) {
    Serial.printf("[WiFi] mDNS started: http://%s.local\n", MDNS_HOSTNAME);
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("[WiFi] mDNS failed to start");
  }
  
  // Setup web server routes
  setupWebServer();
  server.begin();
  Serial.println("[WiFi] Web server started on port 80");
  
  // Start MQTT for remote control
  mqtt.begin();
  
  printMenu();
}

// =============================================
// LOOP
// =============================================
void loop() {
  // Handle web server requests
  server.handleClient();
  
  // Handle MQTT messages and keepalive
  mqtt.loop();
  
  // Check for user input from Serial Monitor
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      handleSerialCommand(input);
    }
  }
  
  // Check for RS-485 responses from pump
  if (Serial2.available()) {
    delay(50);  // Wait for full packet to arrive
    rxLen = 0;
    while (Serial2.available() && rxLen < sizeof(rxBuffer)) {
      rxBuffer[rxLen++] = Serial2.read();
    }
    
    if (rxLen > 0) {
      Serial.println("\n--- PUMP RESPONSE ---");
      printPacketHex("RX", rxBuffer, rxLen);
      parseAndDisplayResponse(rxBuffer, rxLen);
      Serial.println("---------------------\n");
      mqtt.publishStatus();  // Push update to cloud
      printMenu();
    }
  }
  
  // Auto-query pump status periodically (when running)
  if (pumpStatus.running && millis() - lastAutoQuery > AUTO_QUERY_INTERVAL) {
    sendStatusQuery();
    lastAutoQuery = millis();
  }
  
  // Keep WiFi alive
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 30000) {  // Try every 30 seconds
      Serial.println("[WiFi] Connection lost, reconnecting...");
      BLESetup::connectSaved();
      lastReconnect = millis();
    }
  }
}

// =============================================
// WEB SERVER ROUTES
// =============================================
void setupWebServer() {
  // Serve the HTML control panel (inject device ID for MQTT)
  server.on("/", HTTP_GET, []() {
    String html = String(WEBUI_HTML);
    html.replace("%%DEVICE_ID%%", mqtt.getDeviceId());
    server.send(200, "text/html", html);
  });
  
  // GET /api/status - Returns current pump status as JSON
  server.on("/api/status", HTTP_GET, handleApiStatus);
  
  // POST /api/remote - Set remote control
  server.on("/api/remote", HTTP_POST, []() {
    sendRemoteControl();
    sendJsonResponse(true, "Remote control set");
  });
  
  // POST /api/local - Set local control
  server.on("/api/local", HTTP_POST, []() {
    sendLocalControl();
    sendJsonResponse(true, "Local control set");
  });
  
  // POST /api/start - Start pump motor
  server.on("/api/start", HTTP_POST, []() {
    sendRunPump(true);
    sendJsonResponse(true, "Start command sent");
  });
  
  // POST /api/stop - Stop pump motor
  server.on("/api/stop", HTTP_POST, []() {
    sendRunPump(false);
    sendJsonResponse(true, "Stop command sent");
  });
  
  // POST /api/query - Query pump status
  server.on("/api/query", HTTP_POST, []() {
    sendStatusQuery();
    sendJsonResponse(true, "Status query sent");
  });
  
  // POST /api/rpm?value=XXXX - Set RPM directly
  server.on("/api/rpm", HTTP_POST, []() {
    if (!server.hasArg("value")) {
      sendJsonResponse(false, "Missing 'value' parameter");
      return;
    }
    int rpm = server.arg("value").toInt();
    if (rpm < 450 || rpm > 3450) {
      sendJsonResponse(false, "RPM must be 450-3450");
      return;
    }
    sendSetRPM(rpm);
    char msg[40];
    snprintf(msg, sizeof(msg), "RPM set to %d", rpm);
    sendJsonResponse(true, msg);
  });
  
  // POST /api/fullstart?rpm=XXXX - Full start sequence
  server.on("/api/fullstart", HTTP_POST, []() {
    if (!server.hasArg("rpm")) {
      sendJsonResponse(false, "Missing 'rpm' parameter");
      return;
    }
    int rpm = server.arg("rpm").toInt();
    if (rpm < 450 || rpm > 3450) {
      sendJsonResponse(false, "RPM must be 450-3450");
      return;
    }
    runFullSpeedSequence(rpm);
    char msg[50];
    snprintf(msg, sizeof(msg), "Full start sequence sent (%d RPM)", rpm);
    sendJsonResponse(true, msg);
  });
  
  // POST /api/fullstop - Full stop sequence
  server.on("/api/fullstop", HTTP_POST, []() {
    runFullStopSequence();
    sendJsonResponse(true, "Full stop sequence sent");
  });
  
  // GET /wifi/reset - Clear saved WiFi credentials and reboot into BLE setup mode
  server.on("/wifi/reset", HTTP_GET, []() {
    server.send(200, "text/html",
      "<html><body style='background:#0f172a;color:#e2e8f0;font-family:sans-serif;"
      "display:flex;justify-content:center;align-items:center;height:100vh'>"
      "<div style='text-align:center'><h1 style='color:#f59e0b'>WiFi Reset</h1>"
      "<p>Credentials cleared. ESP32 is restarting into Bluetooth setup mode.</p>"
      "<p style='color:#64748b;margin-top:16px'>Open Chrome on your PC or Android to reconfigure via Bluetooth.</p>"
      "</div></body></html>");
    delay(1000);
    BLESetup::clearCredentials();
    delay(500);
    ESP.restart();
  });
  
  // GET /wifi/info - Show current WiFi info
  server.on("/wifi/info", HTTP_GET, []() {
    char json[256];
    snprintf(json, sizeof(json),
      "{\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"mac\":\"%s\"}",
      BLESetup::getSavedSSID().c_str(),
      WiFi.localIP().toString().c_str(),
      WiFi.RSSI(),
      WiFi.macAddress().c_str());
    server.send(200, "application/json", json);
  });
}

// =============================================
// JSON HELPERS
// =============================================
void sendJsonResponse(bool success, const char* message) {
  char json[128];
  snprintf(json, sizeof(json), "{\"success\":%s,\"message\":\"%s\"}",
           success ? "true" : "false", message);
  server.send(200, "application/json", json);
}

void handleApiStatus() {
  char json[384];
  snprintf(json, sizeof(json),
    "{"
    "\"running\":%s,"
    "\"rpm\":%d,"
    "\"watts\":%d,"
    "\"gpm\":%d,"
    "\"mode\":\"%s\","
    "\"error\":%d,"
    "\"drive\":%d,"
    "\"timer\":%d,"
    "\"hour\":%d,"
    "\"minute\":%d,"
    "\"remote\":%s,"
    "\"valid\":%s,"
    "\"age\":%lu,"
    "\"ssid\":\"%s\","
    "\"ip\":\"%s\","
    "\"rssi\":%d,"
    "\"deviceId\":\"%s\","
    "\"mqttConnected\":%s"
    "}",
    pumpStatus.running ? "true" : "false",
    pumpStatus.rpm,
    pumpStatus.watts,
    pumpStatus.gpm,
    modeName(pumpStatus.mode),
    pumpStatus.errCode,
    pumpStatus.drive,
    pumpStatus.timer,
    pumpStatus.hour,
    pumpStatus.minute,
    remoteControlActive ? "true" : "false",
    pumpStatus.valid ? "true" : "false",
    pumpStatus.valid ? (millis() - pumpStatus.lastUpdate) / 1000 : 0,
    BLESetup::getSavedSSID().c_str(),
    WiFi.localIP().toString().c_str(),
    WiFi.RSSI(),
    mqtt.getDeviceId().c_str(),
    mqtt.isConnected() ? "true" : "false"
  );
  server.send(200, "application/json", json);
}

// =============================================
// SERIAL MONITOR COMMAND HANDLER
// =============================================
void handleSerialCommand(String input) {
  // Special text commands
  if (input.equalsIgnoreCase("reset") || input.equalsIgnoreCase("wifi reset")) {
    Serial.println("\n[WiFi] Clearing saved credentials and restarting...");
    Serial.println("[WiFi] BLE setup will start on next boot (use Chrome on PC or Android).");
    BLESetup::clearCredentials();
    delay(500);
    ESP.restart();
    return;
  }
  
  int cmd = input.toInt();
  
  switch (cmd) {
    case 1:  sendRemoteControl(); break;
    case 2:  sendLocalControl(); break;
    case 3:  sendRunPump(true); break;
    case 4:  sendRunPump(false); break;
    case 5:  sendStatusQuery(); break;
    case 6: {
      Serial.println("Enter RPM (450-3450):");
      while (!Serial.available()) { delay(10); }
      String rpmStr = Serial.readStringUntil('\n');
      rpmStr.trim();
      int rpm = rpmStr.toInt();
      if (rpm >= 450 && rpm <= 3450) {
        sendSetRPM(rpm);
      } else {
        Serial.println("ERROR: RPM must be 450-3450");
      }
      break;
    }
    case 7:  sendSetMode(MODE_SPEED_1); break;
    case 8:  sendSetMode(MODE_SPEED_2); break;
    case 9:  sendSetMode(MODE_SPEED_3); break;
    case 10: sendSetMode(MODE_SPEED_4); break;
    case 11: {
      Serial.println("Enter RPM (450-3450):");
      while (!Serial.available()) { delay(10); }
      String rpmStr = Serial.readStringUntil('\n');
      rpmStr.trim();
      int rpm = rpmStr.toInt();
      if (rpm >= 450 && rpm <= 3450) {
        runFullSpeedSequence(rpm);
      } else {
        Serial.println("ERROR: RPM must be 450-3450");
      }
      break;
    }
    case 12: runFullStopSequence(); break;
    default:
      Serial.println("Unknown command. Enter 1-12.");
      printMenu();
      break;
  }
}

// =============================================
// SEND: Set Remote Control (CMD 0x04)
// nodejs-poolController: action:4, payload:[255]
// =============================================
void sendRemoteControl() {
  Serial.println("\n>> SENDING: Set Remote Control");
  Serial.println("   nodejs-poolController: action:4, payload:[255]");
  
  uint8_t data[] = { CTRL_REMOTE };  // 0xFF
  size_t len = pentairBuildPacket(txBuffer, pumpAddr, controllerAddr, CMD_CTRL, data, 1);
  
  sendRS485(txBuffer, len);
  remoteControlActive = true;
}

// =============================================
// SEND: Set Local Control (CMD 0x04)
// nodejs-poolController: action:4, payload:[0]
// =============================================
void sendLocalControl() {
  Serial.println("\n>> SENDING: Set Local Control");
  Serial.println("   nodejs-poolController: action:4, payload:[0]");
  
  uint8_t data[] = { CTRL_LOCAL };  // 0x00
  size_t len = pentairBuildPacket(txBuffer, pumpAddr, controllerAddr, CMD_CTRL, data, 1);
  
  sendRS485(txBuffer, len);
  remoteControlActive = false;
}

// =============================================
// SEND: Run/Stop Pump (CMD 0x06)
// nodejs-poolController: action:6, payload:[10] or [4]
// =============================================
void sendRunPump(bool start) {
  Serial.printf("\n>> SENDING: %s Pump\n", start ? "START" : "STOP");
  Serial.printf("   nodejs-poolController: action:6, payload:[%d]\n", start ? 10 : 4);
  
  uint8_t data[] = { start ? RUN_START : RUN_STOP };  // 0x0A or 0x04
  size_t len = pentairBuildPacket(txBuffer, pumpAddr, controllerAddr, CMD_RUN, data, 1);
  
  sendRS485(txBuffer, len);
}

// =============================================
// SEND: Query Status (CMD 0x07)
// nodejs-poolController: action:7, payload:[]
// =============================================
void sendStatusQuery() {
  Serial.println("\n>> SENDING: Status Query");
  Serial.println("   nodejs-poolController: action:7, payload:[]");
  
  size_t len = pentairBuildPacket(txBuffer, pumpAddr, controllerAddr, CMD_STATUS, NULL, 0);
  
  sendRS485(txBuffer, len);
}

// =============================================
// SEND: Set RPM directly (CMD 0x01, Register 0x02C4)
// nodejs-poolController: action:1, payload:[2, 196, rpm_hi, rpm_lo]
// =============================================
void sendSetRPM(uint16_t rpm) {
  Serial.printf("\n>> SENDING: Set RPM = %d (direct, register 0x02C4)\n", rpm);
  Serial.printf("   nodejs-poolController: action:1, payload:[2, 196, %d, %d]\n",
                (rpm >> 8) & 0xFF, rpm & 0xFF);
  
  uint8_t data[] = {
    (uint8_t)(REG_SET_RPM >> 8),     // 0x02
    (uint8_t)(REG_SET_RPM & 0xFF),   // 0xC4
    (uint8_t)(rpm >> 8),              // RPM high byte
    (uint8_t)(rpm & 0xFF)             // RPM low byte
  };
  
  size_t len = pentairBuildPacket(txBuffer, pumpAddr, controllerAddr, CMD_WRITE_REG, data, 4);
  sendRS485(txBuffer, len);
}

// =============================================
// SEND: Set Mode (CMD 0x05)
// =============================================
void sendSetMode(uint8_t mode) {
  Serial.printf("\n>> SENDING: Set Mode 0x%02X (%s)\n", mode, modeName(mode));
  
  uint8_t data[] = { mode };
  size_t len = pentairBuildPacket(txBuffer, pumpAddr, controllerAddr, CMD_MODE, data, 1);
  
  sendRS485(txBuffer, len);
}

// =============================================
// FULL SEQUENCE: Set speed and run
// Matches nodejs-poolController's setPumpStateAsync() exactly
// =============================================
void runFullSpeedSequence(uint16_t rpm) {
  Serial.println("\n========================================");
  Serial.printf("  FULL SEQUENCE (nodejs-poolController)\n");
  Serial.printf("  Set pump to %d RPM\n", rpm);
  Serial.println("========================================");
  
  // Step 1: Start motor
  Serial.println("\nStep 1/5: Starting motor...");
  {
    uint8_t data[] = { RUN_START };
    size_t len = pentairBuildPacket(txBuffer, pumpAddr, controllerAddr, CMD_RUN, data, 1);
    sendRS485(txBuffer, len);
  }
  waitForResponse();
  
  // Step 2: Set RPM directly
  Serial.printf("\nStep 2/5: Setting RPM to %d...\n", rpm);
  {
    uint8_t data[] = {
      (uint8_t)(REG_SET_RPM >> 8),
      (uint8_t)(REG_SET_RPM & 0xFF),
      (uint8_t)(rpm >> 8),
      (uint8_t)(rpm & 0xFF)
    };
    size_t len = pentairBuildPacket(txBuffer, pumpAddr, controllerAddr, CMD_WRITE_REG, data, 4);
    sendRS485(txBuffer, len);
  }
  waitForResponse();
  
  // Step 3: Wait 1 second
  Serial.println("\nStep 3/5: Waiting 1 second...");
  delay(1000);
  
  // Step 4: Request status
  Serial.println("\nStep 4/5: Requesting pump status...");
  {
    size_t len = pentairBuildPacket(txBuffer, pumpAddr, controllerAddr, CMD_STATUS, NULL, 0);
    sendRS485(txBuffer, len);
  }
  waitForResponse();
  
  // Step 5: Set remote control
  Serial.println("\nStep 5/5: Setting remote control...");
  {
    uint8_t data[] = { CTRL_REMOTE };
    size_t len = pentairBuildPacket(txBuffer, pumpAddr, controllerAddr, CMD_CTRL, data, 1);
    sendRS485(txBuffer, len);
  }
  waitForResponse();
  
  Serial.println("\n========================================");
  Serial.printf("  SEQUENCE COMPLETE: Pump set to %d RPM\n", rpm);
  Serial.println("========================================\n");
  
  remoteControlActive = true;
  lastAutoQuery = millis();  // Reset auto-query timer
}

// =============================================
// FULL SEQUENCE: Stop pump
// =============================================
void runFullStopSequence() {
  Serial.println("\n========================================");
  Serial.println("  FULL SEQUENCE: Stop pump");
  Serial.println("========================================");
  
  // Step 1: Stop motor
  Serial.println("\nStep 1/2: Stopping motor...");
  {
    uint8_t data[] = { RUN_STOP };
    size_t len = pentairBuildPacket(txBuffer, pumpAddr, controllerAddr, CMD_RUN, data, 1);
    sendRS485(txBuffer, len);
  }
  waitForResponse();
  
  // Step 2: Return to local control
  Serial.println("\nStep 2/2: Returning to local control...");
  {
    uint8_t data[] = { CTRL_LOCAL };
    size_t len = pentairBuildPacket(txBuffer, pumpAddr, controllerAddr, CMD_CTRL, data, 1);
    sendRS485(txBuffer, len);
  }
  waitForResponse();
  
  Serial.println("\n========================================");
  Serial.println("  SEQUENCE COMPLETE: Pump stopped");
  Serial.println("========================================\n");
  
  remoteControlActive = false;
}

// =============================================
// WAIT FOR PUMP RESPONSE
// =============================================
void waitForResponse() {
  unsigned long start = millis();
  
  while (millis() - start < CMD_RESPONSE_TIMEOUT) {
    if (Serial2.available()) {
      delay(50);  // Wait for full packet
      rxLen = 0;
      while (Serial2.available() && rxLen < sizeof(rxBuffer)) {
        rxBuffer[rxLen++] = Serial2.read();
      }
      
      if (rxLen > 0) {
        printPacketHex("  RX", rxBuffer, rxLen);
        parseAndDisplayResponse(rxBuffer, rxLen);
        return;
      }
    }
    delay(10);
  }
  
  Serial.println("  WARNING: No response from pump (timeout)");
}

// =============================================
// RS-485 SEND
// =============================================
void sendRS485(uint8_t* data, size_t length) {
  printPacketHex("  TX", data, length);
  
  digitalWrite(RS485_DE_RE_PIN, HIGH);
  delayMicroseconds(100);
  
  Serial2.write(data, length);
  Serial2.flush();
  
  delayMicroseconds(100);
  digitalWrite(RS485_DE_RE_PIN, LOW);
}

// =============================================
// PARSE AND DISPLAY RESPONSE
// (also updates pumpStatus struct for web UI)
// =============================================
void parseAndDisplayResponse(uint8_t* data, size_t length) {
  int msgStart = pentairFindMessage(data, length);
  if (msgStart < 0) {
    Serial.println("  Could not find valid Pentair preamble (FF 00 FF A5)");
    return;
  }
  
  uint8_t* pkt = &data[msgStart];
  size_t remaining = length - msgStart;
  
  if (remaining < PENTAIR_MIN_PKT_LEN) {
    Serial.println("  Packet too short");
    return;
  }
  
  uint8_t version = pkt[PKT_IDX_VER];
  uint8_t src     = pkt[PKT_IDX_SRC];
  uint8_t dst     = pkt[PKT_IDX_DST];
  uint8_t cmd     = pkt[PKT_IDX_CMD];
  uint8_t dataLen = pkt[PKT_IDX_LEN];
  
  size_t totalLen = pentairPacketLength(pkt);
  
  bool checksumOK = false;
  if (totalLen <= remaining) {
    checksumOK = pentairVerifyChecksum(pkt, totalLen);
  }
  
  Serial.printf("  Ver: 0x%02X  Src: 0x%02X  Dst: 0x%02X  Cmd: 0x%02X  Len: %d  Checksum: %s\n",
                version, src, dst, cmd, dataLen, checksumOK ? "OK" : "BAD");
  
  if (dataLen > 0 && PKT_IDX_DATA + dataLen <= remaining) {
    uint8_t* respData = &pkt[PKT_IDX_DATA];
    
    switch (cmd) {
      case CMD_CTRL:
        Serial.printf("  >> Control mode: %s\n",
                      respData[0] == CTRL_REMOTE ? "REMOTE" : "LOCAL");
        break;
        
      case CMD_RUN:
        Serial.printf("  >> Pump: %s\n",
                      respData[0] == RUN_START ? "STARTED" : "STOPPED");
        // Update status from run/stop acknowledgment
        pumpStatus.running = (respData[0] == RUN_START);
        pumpStatus.lastUpdate = millis();
        break;
        
      case CMD_MODE:
        Serial.printf("  >> Mode set to: 0x%02X (%s)\n", respData[0], modeName(respData[0]));
        pumpStatus.mode = respData[0];
        pumpStatus.lastUpdate = millis();
        break;
        
      case CMD_WRITE_REG:
        if (dataLen >= 2) {
          uint16_t val = ((uint16_t)respData[0] << 8) | respData[1];
          Serial.printf("  >> Register write confirmed, value: %d (0x%04X)\n", val, val);
        }
        break;
        
      case CMD_STATUS:
        if (dataLen >= STAT_DATA_LEN) {
          uint8_t  runState = respData[STAT_RUN];
          uint8_t  mode     = respData[STAT_MODE];
          uint8_t  drive    = respData[STAT_DRIVE];
          uint16_t watts    = ((uint16_t)respData[STAT_PWR_HI] << 8) | respData[STAT_PWR_LO];
          uint16_t rpm      = ((uint16_t)respData[STAT_RPM_HI] << 8) | respData[STAT_RPM_LO];
          uint8_t  gpm      = respData[STAT_GPM];
          uint8_t  errCode  = respData[STAT_ERR];
          uint8_t  timer    = respData[STAT_TIMER];
          uint8_t  hour     = respData[STAT_CLK_HOUR];
          uint8_t  minute   = respData[STAT_CLK_MIN];
          
          // Update pumpStatus struct for web UI
          pumpStatus.valid      = true;
          pumpStatus.running    = (runState == RUN_START);
          pumpStatus.rpm        = rpm;
          pumpStatus.watts      = watts;
          pumpStatus.gpm        = gpm;
          pumpStatus.mode       = mode;
          pumpStatus.errCode    = errCode;
          pumpStatus.drive      = drive;
          pumpStatus.timer      = timer;
          pumpStatus.hour       = hour;
          pumpStatus.minute     = minute;
          pumpStatus.lastUpdate = millis();
          
          Serial.println("  ┌─────────────────────────────────┐");
          Serial.printf( "  │ Status: %-8s  Mode: %-8s │\n",
                         runState == RUN_START ? "RUNNING" : "STOPPED",
                         modeName(mode));
          Serial.printf( "  │ Speed:  %-5d RPM               │\n", rpm);
          Serial.printf( "  │ Power:  %-5d Watts             │\n", watts);
          Serial.printf( "  │ Flow:   %-3d GPM                 │\n", gpm);
          Serial.printf( "  │ Drive:  0x%02X                     │\n", drive);
          Serial.printf( "  │ Error:  0x%02X %s│\n", errCode,
                         errCode == 0 ? "(none)                 " : "(ERROR!)               ");
          Serial.printf( "  │ Timer:  %d min                  │\n", timer);
          Serial.printf( "  │ Clock:  %02d:%02d                    │\n", hour, minute);
          Serial.println("  └─────────────────────────────────┘");
        } else {
          Serial.println("  >> Status response (short data)");
          for (int i = 0; i < dataLen; i++) {
            Serial.printf("  Byte %d: 0x%02X\n", i, respData[i]);
          }
        }
        break;
        
      default:
        Serial.print("  >> Data: ");
        for (int i = 0; i < dataLen; i++) {
          Serial.printf("%02X ", respData[i]);
        }
        Serial.println();
        break;
    }
  }
}

// =============================================
// MODE NAME HELPER
// =============================================
const char* modeName(uint8_t mode) {
  switch (mode) {
    case MODE_FILTER:     return "Filter";
    case MODE_MANUAL:     return "Manual";
    case MODE_SPEED_1:    return "Speed 1";
    case MODE_SPEED_2:    return "Speed 2";
    case MODE_SPEED_3:    return "Speed 3";
    case MODE_SPEED_4:    return "Speed 4";
    case MODE_FEATURE_1:  return "Feature1";
    case MODE_EXT_PROG_1: return "ExtPrg1";
    case MODE_EXT_PROG_2: return "ExtPrg2";
    case MODE_EXT_PROG_3: return "ExtPrg3";
    case MODE_EXT_PROG_4: return "ExtPrg4";
    default:              return "Unknown";
  }
}

// =============================================
// PRINT PACKET HEX
// =============================================
void printPacketHex(const char* prefix, uint8_t* data, size_t length) {
  Serial.printf("%s [%d bytes]: ", prefix, length);
  for (size_t i = 0; i < length; i++) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();
}

// =============================================
// PRINT BANNER
// =============================================
void printBanner() {
  Serial.println("\n==============================================");
  Serial.println("  FlexPool - Pentair Pump Controller");
  Serial.println("  WiFi + RS-485 Remote Control");
  Serial.println("==============================================");
  Serial.printf( "  Controller: 0x%02X  Pump: 0x%02X\n",
                 controllerAddr, pumpAddr);
  Serial.println("  RS-485: 9600 baud, 8N1");
  Serial.println("  Protocol: Exact nodejs-poolController");
  Serial.println("==============================================\n");
}

// =============================================
// PRINT MENU (Serial Monitor only)
// =============================================
void printMenu() {
  Serial.println("========================================");
  Serial.println("  SERIAL COMMANDS:");
  Serial.println("========================================");
  Serial.println("  1  - Set REMOTE control");
  Serial.println("  2  - Set LOCAL control");
  Serial.println("  3  - Start pump");
  Serial.println("  4  - Stop pump");
  Serial.println("  5  - Query status");
  Serial.println("  6  - Set RPM (direct)");
  Serial.println("  7-10 - Set mode (Speed 1-4)");
  Serial.println("  11 - FULL: Set RPM and run");
  Serial.println("  12 - FULL: Stop pump");
  Serial.println("  --- WiFi ---");
  Serial.println("  reset - Clear WiFi & restart Bluetooth setup");
  Serial.println("========================================");
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("  WiFi: http://flexpool.local or http://%s\n",
                  WiFi.localIP().toString().c_str());
    Serial.printf("  Network: %s (%d dBm)\n",
                  BLESetup::getSavedSSID().c_str(), WiFi.RSSI());
    Serial.printf("  MQTT: %s  Device ID: %s\n",
                  mqtt.isConnected() ? "Connected" : "Disconnected",
                  mqtt.getDeviceId().c_str());
  } else {
    Serial.println("  WiFi: Not connected");
  }
  
  Serial.println("========================================");
  Serial.print("Enter command: ");
}
