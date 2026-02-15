/*
 * =============================================
 * PUMP SIMULATOR ESP32 - Exact Pentair Protocol
 * =============================================
 * 
 * Upload this to your second ESP32.
 * Connect to Controller ESP32 via RS-485 (MAX3485 module).
 * Optionally connect USB to computer to see debug output.
 * 
 * This simulates a real Pentair IntelliFlo VS pump using
 * the exact protocol as reverse-engineered by the community:
 *   - github.com/tagyoureit/nodejs-poolController
 *   - github.com/eriedl/pavsp_rs485_examples
 * 
 * PROTOCOL BEHAVIOR:
 *   - Only responds when addressed to 0x60 (pump 1)
 *   - Validates checksums on incoming packets
 *   - Implements all major IntelliFlo commands:
 *     * Remote/Local control (0x04)
 *     * Mode setting (0x05)
 *     * Run/Stop (0x06)
 *     * Status query (0x07) - returns full 15-byte status
 *     * Register write (0x01) - program RPMs, select programs
 *   - Simulates gradual speed changes (acceleration/deceleration)
 *   - Tracks power consumption based on RPM
 * 
 * Wiring:
 *   ESP32 GPIO 18 --> MAX3485 DI
 *   ESP32 GPIO 19 <-- MAX3485 RO
 *   ESP32 GPIO 4  --> MAX3485 DE & RE (tie together)
 *   ESP32 GND     --> MAX3485 GND
 *   ESP32 3.3V    --> MAX3485 VCC
 *   MAX3485 A     --> (wire to Controller ESP32's MAX3485 A)
 *   MAX3485 B     --> (wire to Controller ESP32's MAX3485 B)
 *   120 ohm resistor between A and B (termination)
 */

#include <HardwareSerial.h>
#include "PentairProtocol.h"

// =============================================
// PIN CONFIGURATION
// =============================================
#define RS485_TX_PIN    18
#define RS485_RX_PIN    19
#define RS485_DE_RE_PIN 4

// =============================================
// COMMUNICATION SETTINGS
// =============================================
#define RS485_BAUD  9600
#define USB_BAUD    115200

// =============================================
// PUMP STATE (simulated IntelliFlo VS)
// =============================================
struct PumpState {
  uint8_t  myAddress    = ADDR_PUMP_1;      // 0x60 - Our address
  uint8_t  controlMode  = CTRL_LOCAL;        // Local or Remote
  uint8_t  runState     = RUN_STOP;          // Running or stopped
  uint8_t  mode         = MODE_FILTER;       // Current mode
  uint8_t  driveState   = DRIVE_READY;       // Drive status
  uint16_t currentRPM   = 0;                 // Actual current RPM
  uint16_t targetRPM    = 0;                 // Target RPM
  uint16_t powerWatts   = 0;                 // Power consumption
  uint8_t  flowGPM      = 0;                 // Flow rate
  uint8_t  ppcLevel     = 0;                 // PPC/chlorinator
  uint8_t  errorCode    = 0x00;              // Error code (0 = no error)
  uint8_t  timerMin     = 0;                 // Timer remaining
  uint8_t  clockHour    = 0;                 // Clock hour
  uint8_t  clockMin     = 0;                 // Clock minute
  
  // External program RPM settings
  uint16_t extProgRPM[5] = {0, 1000, 2000, 2500, 3450};  // Index 0 unused
  
  // External program selection
  uint16_t extProgSelect = EPRG_OFF;         // Which ext program is selected
  
  // Speed presets (built-in)
  uint16_t speedPreset[5] = {0, 750, 1500, 2350, 3110};   // Index 0 unused
} pump;

// =============================================
// BUFFERS
// =============================================
uint8_t rxBuffer[96];
uint8_t txBuffer[48];

// =============================================
// TIMING
// =============================================
unsigned long lastSpeedUpdate = 0;
unsigned long lastStatusPrint = 0;

// =============================================
// SETUP
// =============================================
void setup() {
  Serial.begin(USB_BAUD);
  delay(3000);  // Wait 3 seconds so Serial Monitor can connect
  
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW);  // Start in receive mode
  
  Serial2.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  
  // Set simulated clock
  pump.clockHour = 12;
  pump.clockMin = 0;
  
  Serial.println("\n=============================================");
  Serial.println("  Pentair IntelliFlo VS Pump Simulator");
  Serial.println("  (Exact Protocol Implementation)");
  Serial.println("=============================================");
  Serial.printf( "  Address:  0x%02X (Pump 1)\n", pump.myAddress);
  Serial.println("  Status:   STOPPED / LOCAL control");
  Serial.println("  Speed:    0 RPM");
  Serial.println("  Protocol: FF 00 FF A5 [VER DST SRC CMD LEN DATA... CHK CHK]");
  Serial.println("=============================================");
  Serial.println("  Waiting for commands from Controller...\n");
}

// =============================================
// LOOP
// =============================================
void loop() {
  // Check for incoming RS-485 packets
  if (Serial2.available()) {
    delay(200);  // Wait 200ms for full packet to arrive (Controller sends slowly) (at 9600 baud, 12 bytes ≈ 12ms)
    size_t len = 0;
    while (Serial2.available() && len < sizeof(rxBuffer)) {
      rxBuffer[len++] = Serial2.read();
    }
    
    if (len > 0) {
      processIncoming(rxBuffer, len);
    }
  }
  
  // Simulate pump physics (gradual speed changes)
  updatePumpSimulation();
  
  // Periodic status print (always prints so you know it's alive)
  if (millis() - lastStatusPrint > 5000) {
    lastStatusPrint = millis();
    if (pump.runState == RUN_START) {
      Serial.printf("[PUMP] RPM: %d/%d  Power: %dW  Flow: %d GPM  Mode: %s\n",
                    pump.currentRPM, pump.targetRPM, pump.powerWatts,
                    pump.flowGPM, modeName(pump.mode));
    } else {
      Serial.println("[PUMP] Idle - waiting for commands on RS-485...");
    }
  }
  
  delay(10);
}

// =============================================
// PROCESS INCOMING PACKET
// =============================================
void processIncoming(uint8_t* data, size_t length) {
  // Always print raw bytes first (for debugging)
  Serial.printf("\n>> RX RAW [%d bytes]: ", length);
  for (size_t i = 0; i < length; i++) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();
  
  // Find the Pentair preamble
  int msgStart = pentairFindMessage(data, length);
  if (msgStart < 0) {
    Serial.println(">> No valid Pentair preamble (FF 00 FF A5) found in above bytes");
    return;
  }
  
  uint8_t* pkt = &data[msgStart];
  size_t remaining = length - msgStart;
  
  if (remaining < PENTAIR_MIN_PKT_LEN) {
    Serial.println(">> RX: Packet too short");
    return;
  }
  
  uint8_t dst     = pkt[PKT_IDX_DST];
  uint8_t src     = pkt[PKT_IDX_SRC];
  uint8_t cmd     = pkt[PKT_IDX_CMD];
  uint8_t dataLen = pkt[PKT_IDX_LEN];
  
  size_t totalLen = pentairPacketLength(pkt);
  
  // Print received packet
  Serial.printf("\n>> RX [%d bytes]: ", length);
  for (size_t i = 0; i < length; i++) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();
  Serial.printf("   Dst: 0x%02X  Src: 0x%02X  Cmd: 0x%02X  Len: %d\n",
                dst, src, cmd, dataLen);
  
  // Check if addressed to us
  if (dst != pump.myAddress) {
    Serial.printf("   Not for us (addressed to 0x%02X), ignoring\n", dst);
    return;
  }
  
  // Verify checksum
  if (totalLen <= remaining) {
    if (!pentairVerifyChecksum(pkt, totalLen)) {
      Serial.println("   BAD CHECKSUM - dropping packet");
      return;
    }
    Serial.println("   Checksum: OK");
  }
  
  // Get data pointer
  uint8_t* cmdData = &pkt[PKT_IDX_DATA];
  
  // Process command
  switch (cmd) {
    case CMD_CTRL:      // 0x04 - Remote/Local Control
      handleCtrl(src, cmdData, dataLen);
      break;
      
    case CMD_MODE:      // 0x05 - Set Mode
      handleMode(src, cmdData, dataLen);
      break;
      
    case CMD_RUN:       // 0x06 - Run/Stop
      handleRun(src, cmdData, dataLen);
      break;
      
    case CMD_STATUS:    // 0x07 - Status Query
      handleStatus(src);
      break;
      
    case CMD_WRITE_REG: // 0x01 - Register Write
      handleRegWrite(src, cmdData, dataLen);
      break;
      
    default:
      Serial.printf("   UNKNOWN command: 0x%02X\n", cmd);
      break;
  }
}

// =============================================
// HANDLE: Remote/Local Control (CMD 0x04)
// =============================================
void handleCtrl(uint8_t src, uint8_t* data, uint8_t dataLen) {
  if (dataLen < 1) return;
  
  uint8_t oldMode = pump.controlMode;
  pump.controlMode = data[0];
  
  Serial.printf("   CMD 0x04 CTRL: %s -> %s\n",
                oldMode == CTRL_REMOTE ? "REMOTE" : "LOCAL",
                pump.controlMode == CTRL_REMOTE ? "REMOTE" : "LOCAL");
  
  // Send confirmation: echo back the control mode
  uint8_t respData[] = { pump.controlMode };
  size_t len = pentairBuildPacket(txBuffer, src, pump.myAddress, CMD_CTRL, respData, 1);
  sendRS485(txBuffer, len);
}

// =============================================
// HANDLE: Set Mode (CMD 0x05)
// =============================================
void handleMode(uint8_t src, uint8_t* data, uint8_t dataLen) {
  if (dataLen < 1) return;
  
  // Only accept if in remote control mode
  if (pump.controlMode != CTRL_REMOTE) {
    Serial.println("   REJECTED: Not in remote control mode");
    return;
  }
  
  uint8_t oldMode = pump.mode;
  pump.mode = data[0];
  
  Serial.printf("   CMD 0x05 MODE: %s -> %s\n",
                modeName(oldMode), modeName(pump.mode));
  
  // Update target RPM based on mode
  updateTargetFromMode();
  
  // Send confirmation: echo back the mode
  uint8_t respData[] = { pump.mode };
  size_t len = pentairBuildPacket(txBuffer, src, pump.myAddress, CMD_MODE, respData, 1);
  sendRS485(txBuffer, len);
}

// =============================================
// HANDLE: Run/Stop (CMD 0x06)
// =============================================
void handleRun(uint8_t src, uint8_t* data, uint8_t dataLen) {
  if (dataLen < 1) return;
  
  uint8_t oldState = pump.runState;
  pump.runState = data[0];
  
  Serial.printf("   CMD 0x06 RUN: %s -> %s\n",
                oldState == RUN_START ? "RUNNING" : "STOPPED",
                pump.runState == RUN_START ? "RUNNING" : "STOPPED");
  
  if (pump.runState == RUN_STOP) {
    pump.targetRPM = 0;
    // If stopped, reset mode to filter
    if (pump.mode >= MODE_EXT_PROG_1 && pump.mode <= MODE_EXT_PROG_4) {
      pump.mode = MODE_FILTER;
    }
  } else if (pump.runState == RUN_START) {
    updateTargetFromMode();
  }
  
  // Send confirmation: echo back the run state
  uint8_t respData[] = { pump.runState };
  size_t len = pentairBuildPacket(txBuffer, src, pump.myAddress, CMD_RUN, respData, 1);
  sendRS485(txBuffer, len);
}

// =============================================
// HANDLE: Status Query (CMD 0x07)
// =============================================
void handleStatus(uint8_t src) {
  // Status queries are always answered (even in local mode)
  Serial.printf("   CMD 0x07 STATUS: Sending full status (%d bytes)\n", STAT_DATA_LEN);
  
  // Build the 15-byte status response (exact Pentair format)
  uint8_t statusData[STAT_DATA_LEN];
  statusData[STAT_RUN]      = pump.runState;
  statusData[STAT_MODE]     = pump.mode;
  statusData[STAT_DRIVE]    = pump.driveState;
  statusData[STAT_PWR_HI]   = (pump.powerWatts >> 8) & 0xFF;
  statusData[STAT_PWR_LO]   = pump.powerWatts & 0xFF;
  statusData[STAT_RPM_HI]   = (pump.currentRPM >> 8) & 0xFF;
  statusData[STAT_RPM_LO]   = pump.currentRPM & 0xFF;
  statusData[STAT_GPM]      = pump.flowGPM;
  statusData[STAT_PPC]      = pump.ppcLevel;
  statusData[STAT_BYTE_9]   = 0x00;
  statusData[STAT_ERR]      = pump.errorCode;
  statusData[STAT_BYTE_11]  = 0x00;
  statusData[STAT_TIMER]    = pump.timerMin;
  statusData[STAT_CLK_HOUR] = pump.clockHour;
  statusData[STAT_CLK_MIN]  = pump.clockMin;
  
  size_t len = pentairBuildPacket(txBuffer, src, pump.myAddress, CMD_STATUS,
                                   statusData, STAT_DATA_LEN);
  sendRS485(txBuffer, len);
}

// =============================================
// HANDLE: Register Write (CMD 0x01)
// =============================================
void handleRegWrite(uint8_t src, uint8_t* data, uint8_t dataLen) {
  if (dataLen < 4) {
    Serial.println("   CMD 0x01 REG: Data too short (need 4 bytes)");
    return;
  }
  
  uint16_t regAddr = ((uint16_t)data[0] << 8) | data[1];
  uint16_t regVal  = ((uint16_t)data[2] << 8) | data[3];
  
  Serial.printf("   CMD 0x01 REG: Addr=0x%04X  Value=0x%04X (%d)\n",
                regAddr, regVal, regVal);
  
  // Process register write
  switch (regAddr) {
    case REG_SET_RPM:
      // This is what nodejs-poolController uses to set VS pump speed
      pump.targetRPM = regVal;
      pump.mode = MODE_MANUAL;
      Serial.printf("   >> Set RPM = %d (direct, from nodejs-poolController)\n", regVal);
      break;
      
    case REG_SET_GPM:
      pump.flowGPM = regVal;
      Serial.printf("   >> Set GPM = %d\n", regVal);
      break;
      
    case REG_EXT_PROG:
      pump.extProgSelect = regVal;
      Serial.printf("   >> External program select = 0x%04X", regVal);
      if (regVal == EPRG_OFF) Serial.println(" (OFF)");
      else if (regVal == EPRG_1) Serial.println(" (Program 1)");
      else if (regVal == EPRG_2) Serial.println(" (Program 2)");
      else if (regVal == EPRG_3) Serial.println(" (Program 3)");
      else if (regVal == EPRG_4) Serial.println(" (Program 4)");
      else Serial.println();
      
      // Update mode based on program selection
      if (regVal == EPRG_OFF) { /* keep current mode */ }
      else if (regVal == EPRG_1) pump.mode = MODE_EXT_PROG_1;
      else if (regVal == EPRG_2) pump.mode = MODE_EXT_PROG_2;
      else if (regVal == EPRG_3) pump.mode = MODE_EXT_PROG_3;
      else if (regVal == EPRG_4) pump.mode = MODE_EXT_PROG_4;
      
      updateTargetFromMode();
      break;
      
    case REG_EXT_PROG_1_RPM:
      pump.extProgRPM[1] = regVal;
      Serial.printf("   >> Ext Program 1 RPM = %d\n", regVal);
      if (pump.mode == MODE_EXT_PROG_1) updateTargetFromMode();
      break;
      
    case REG_EXT_PROG_2_RPM:
      pump.extProgRPM[2] = regVal;
      Serial.printf("   >> Ext Program 2 RPM = %d\n", regVal);
      if (pump.mode == MODE_EXT_PROG_2) updateTargetFromMode();
      break;
      
    case REG_EXT_PROG_3_RPM:
      pump.extProgRPM[3] = regVal;
      Serial.printf("   >> Ext Program 3 RPM = %d\n", regVal);
      if (pump.mode == MODE_EXT_PROG_3) updateTargetFromMode();
      break;
      
    case REG_EXT_PROG_4_RPM:
      pump.extProgRPM[4] = regVal;
      Serial.printf("   >> Ext Program 4 RPM = %d\n", regVal);
      if (pump.mode == MODE_EXT_PROG_4) updateTargetFromMode();
      break;
      
    default:
      Serial.printf("   >> Unknown register: 0x%04X\n", regAddr);
      break;
  }
  
  // Send confirmation: echo back the VALUE only (2 bytes)
  // (Real pumps echo the value, not the register address)
  uint8_t respData[] = {
    (uint8_t)(regVal >> 8),
    (uint8_t)(regVal & 0xFF)
  };
  size_t len = pentairBuildPacket(txBuffer, src, pump.myAddress, CMD_WRITE_REG, respData, 2);
  sendRS485(txBuffer, len);
}

// =============================================
// UPDATE TARGET RPM BASED ON MODE
// =============================================
void updateTargetFromMode() {
  if (pump.runState != RUN_START) return;  // Only update if running
  
  switch (pump.mode) {
    case MODE_FILTER:
      pump.targetRPM = 0;  // Filter mode: controlled by schedule
      break;
    case MODE_MANUAL:
      // Manual mode keeps whatever RPM was set
      break;
    case MODE_SPEED_1:
      pump.targetRPM = pump.speedPreset[1];
      break;
    case MODE_SPEED_2:
      pump.targetRPM = pump.speedPreset[2];
      break;
    case MODE_SPEED_3:
      pump.targetRPM = pump.speedPreset[3];
      break;
    case MODE_SPEED_4:
      pump.targetRPM = pump.speedPreset[4];
      break;
    case MODE_EXT_PROG_1:
      pump.targetRPM = pump.extProgRPM[1];
      break;
    case MODE_EXT_PROG_2:
      pump.targetRPM = pump.extProgRPM[2];
      break;
    case MODE_EXT_PROG_3:
      pump.targetRPM = pump.extProgRPM[3];
      break;
    case MODE_EXT_PROG_4:
      pump.targetRPM = pump.extProgRPM[4];
      break;
    default:
      break;
  }
  
  Serial.printf("   >> Target RPM updated to %d (mode: %s)\n",
                pump.targetRPM, modeName(pump.mode));
}

// =============================================
// SIMULATE PUMP PHYSICS
// =============================================
void updatePumpSimulation() {
  if (millis() - lastSpeedUpdate < 200) return;
  lastSpeedUpdate = millis();
  
  uint16_t prevRPM = pump.currentRPM;
  
  // Gradual acceleration/deceleration
  if (pump.currentRPM < pump.targetRPM) {
    // Accelerate: +100 RPM per 200ms step
    pump.currentRPM += 100;
    if (pump.currentRPM > pump.targetRPM) pump.currentRPM = pump.targetRPM;
  }
  else if (pump.currentRPM > pump.targetRPM) {
    // Decelerate: -150 RPM per 200ms step (decel is faster)
    if (pump.currentRPM > 150) {
      pump.currentRPM -= 150;
    } else {
      pump.currentRPM = 0;
    }
    if (pump.currentRPM < pump.targetRPM) pump.currentRPM = pump.targetRPM;
  }
  
  // Update derived values
  if (pump.currentRPM == 0) {
    pump.powerWatts = 0;
    pump.flowGPM = 0;
    if (pump.runState == RUN_START && pump.targetRPM == 0) {
      // Pump spun down completely
    }
  } else {
    // Simulate power: roughly proportional to RPM^3 (simplified to linear for sim)
    // Real IntelliFlo: ~40W at 450 RPM, ~2000W at 3450 RPM
    pump.powerWatts = (uint16_t)((float)pump.currentRPM * 2000.0 / 3450.0);
    
    // Simulate flow: roughly proportional to RPM (simplified)
    // Real varies by system, ~15 GPM at 1000 RPM, ~70 GPM at 3450 RPM
    pump.flowGPM = (uint8_t)((float)pump.currentRPM * 70.0 / 3450.0);
  }
  
  // Update simulated clock (increment every minute)
  static unsigned long lastClockUpdate = 0;
  if (millis() - lastClockUpdate > 60000) {
    lastClockUpdate = millis();
    pump.clockMin++;
    if (pump.clockMin >= 60) {
      pump.clockMin = 0;
      pump.clockHour++;
      if (pump.clockHour >= 24) pump.clockHour = 0;
    }
  }
  
  // Print speed changes
  if (prevRPM != pump.currentRPM) {
    Serial.printf("[PUMP] %d RPM -> %d RPM (target: %d)  |  %dW  |  %d GPM  |  %s\n",
                  prevRPM, pump.currentRPM, pump.targetRPM,
                  pump.powerWatts, pump.flowGPM,
                  pump.runState == RUN_START ? "RUNNING" : "STOPPED");
  }
}

// =============================================
// RS-485 SEND
// =============================================
void sendRS485(uint8_t* data, size_t length) {
  Serial.printf("<< TX [%d bytes]: ", length);
  for (size_t i = 0; i < length; i++) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();
  
  // Clear any stale bytes in the receive buffer
  while (Serial2.available()) Serial2.read();
  
  // Enable transmitter
  digitalWrite(RS485_DE_RE_PIN, HIGH);
  delayMicroseconds(500);
  
  // Send 3 sync bytes before actual packet (receiver needs time to lock on)
  uint8_t sync[] = {0xFF, 0xFF, 0xFF};
  Serial2.write(sync, 3);
  Serial2.flush();
  delay(4);
  
  // Send actual packet
  Serial2.write(data, length);
  Serial2.flush();
  
  // Wait for all bytes to physically leave the wire
  unsigned int safetyMs = ((length * 11) / 10) + 5;
  delay(safetyMs);
  
  digitalWrite(RS485_DE_RE_PIN, LOW);   // Switch back to receive mode
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
