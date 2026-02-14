/*
 * =============================================
 * PentairProtocol.h
 * =============================================
 * 
 * EXACT Pentair RS-485 protocol constants.
 * 
 * Based on reverse-engineering from:
 *   - github.com/tagyoureit/nodejs-poolController
 *   - github.com/eriedl/pavsp_rs485_examples (Michael Usner / pab014)
 *   - Community documentation of Pentair IntelliFlo VS protocol
 * 
 * PACKET FORMAT (on the wire):
 *   [FF 00 FF] [A5] [VER] [DST] [SRC] [CMD] [LEN] [DATA...] [CHK_HI CHK_LO]
 *   |_________|  |    |     |     |     |     |      |          |
 *   Preamble   Lead  Ver  Dest  Src  Cmd/  Data   Payload    Checksum
 *   (3 bytes)  Proto      Addr  Addr  CFI   Len              (sum from A5
 *              Byte                                            to end of data)
 *
 * Checksum = sum of all bytes from 0xA5 through last data byte (inclusive)
 *            Stored as 2 bytes: [high_byte] [low_byte]
 */

#ifndef PENTAIR_PROTOCOL_H
#define PENTAIR_PROTOCOL_H

#include <Arduino.h>

// =============================================
// PREAMBLE
// =============================================
// Every Pentair packet starts with this 4-byte sequence
// FF 00 FF A5 (where A5 is actually the start of the message body)
#define PENTAIR_PREAMBLE_LEN  4
const uint8_t PENTAIR_PREAMBLE[] = {0xFF, 0x00, 0xFF, 0xA5};

// =============================================
// PACKET BYTE INDICES (relative to start of full packet including preamble)
// =============================================
#define PKT_IDX_LEAD    3   // 0xA5 - Lead Protocol Byte (start of checksummed region)
#define PKT_IDX_VER     4   // Version byte (usually 0x00)
#define PKT_IDX_DST     5   // Destination address
#define PKT_IDX_SRC     6   // Source address
#define PKT_IDX_CMD     7   // Command / Function Identifier (CFI)
#define PKT_IDX_LEN     8   // Data length
#define PKT_IDX_DATA    9   // First data byte

// Minimum packet size: preamble(4) + ver(1) + dst(1) + src(1) + cmd(1) + len(1) + checksum(2) = 11
#define PENTAIR_MIN_PKT_LEN  11

// Version byte (always 0x00 in observed traffic)
#define PENTAIR_VERSION  0x00

// =============================================
// DEVICE ADDRESSES
// =============================================
// Every device on the Pentair RS-485 bus has an address:
//   0x0F      - Broadcast (controllers use this for status broadcasts)
//   0x10-0x1F - Main controllers (IntelliComII, IntelliTouch, EasyTouch)
//   0x20-0x2F - Remote controllers (wireless remotes, external controllers)
//   0x60-0x6F - Pumps (0x60 = pump 1, 0x61 = pump 2, etc.)

#define ADDR_BROADCAST          0x0F

// Main controllers
#define ADDR_MAIN_CONTROLLER_1  0x10
#define ADDR_MAIN_CONTROLLER_2  0x11

// Remote controllers (WE use this range - acting as a remote controller)
#define ADDR_REMOTE_CONTROLLER  0x20

// Pumps
#define ADDR_PUMP_1             0x60
#define ADDR_PUMP_2             0x61
#define ADDR_PUMP_3             0x62
#define ADDR_PUMP_4             0x63

// =============================================
// COMMANDS (CFI - Command/Function Identifier)
// =============================================
// These are the actual Pentair IntelliFlo command codes

// 0x01 - REGISTER WRITE
// Writes values to pump memory registers (set RPM, select programs, etc.)
// Data format: [REG_HI] [REG_LO] [VAL_HI] [VAL_LO]
#define CMD_WRITE_REG           0x01

// 0x04 - REMOTE/LOCAL CONTROL
// Tells the pump to accept remote commands (required before sending other commands)
// or to return to local (front-panel) control
// Data: 0xFF = remote control, 0x00 = local control
#define CMD_CTRL                0x04
#define CTRL_REMOTE             0xFF
#define CTRL_LOCAL              0x00

// 0x05 - SET MODE
// Sets the pump operating mode (filter, manual, speed presets, ext programs)
// Data: 1 byte mode value
#define CMD_MODE                0x05
#define MODE_FILTER             0x00   // Filter mode
#define MODE_MANUAL             0x01   // Manual mode
#define MODE_SPEED_1            0x02   // Speed preset 1
#define MODE_SPEED_2            0x03   // Speed preset 2
#define MODE_SPEED_3            0x04   // Speed preset 3
#define MODE_SPEED_4            0x05   // Speed preset 4
#define MODE_FEATURE_1          0x06   // Feature 1
#define MODE_EXT_PROG_1         0x09   // External program 1
#define MODE_EXT_PROG_2         0x0A   // External program 2
#define MODE_EXT_PROG_3         0x0B   // External program 3
#define MODE_EXT_PROG_4         0x0C   // External program 4

// 0x06 - RUN/STOP
// Starts or stops the pump motor
// Data: 0x0A = start, 0x04 = stop
#define CMD_RUN                 0x06
#define RUN_START               0x0A
#define RUN_STOP                0x04

// 0x07 - STATUS REQUEST
// Requests current pump status. No data payload.
// Response contains 15 bytes of status information.
#define CMD_STATUS              0x07

// =============================================
// REGISTER ADDRESSES (for CMD_WRITE_REG / 0x01)
// =============================================
// These are 16-bit register addresses in the pump's memory

// Set speed directly (these are what nodejs-poolController uses)
#define REG_SET_RPM             0x02C4   // Set speed in RPM (VS pump) - used by nodejs-poolController
#define REG_SET_GPM             0x02E4   // Set speed in GPM (VF pump)

// External program selection
#define REG_EXT_PROG            0x0321   // Select which external program to run
#define EPRG_OFF                0x0000   // All external programs off
#define EPRG_1                  0x0008   // External program 1
#define EPRG_2                  0x0010   // External program 2
#define EPRG_3                  0x0018   // External program 3
#define EPRG_4                  0x0020   // External program 4

// External program RPM settings
#define REG_EXT_PROG_1_RPM     0x0327   // Set RPM for external program 1
#define REG_EXT_PROG_2_RPM     0x0328   // Set RPM for external program 2
#define REG_EXT_PROG_3_RPM     0x0329   // Set RPM for external program 3
#define REG_EXT_PROG_4_RPM     0x032A   // Set RPM for external program 4

// =============================================
// STATUS RESPONSE DATA INDICES
// (relative to start of data portion, i.e., PKT_IDX_DATA)
// =============================================
// When pump responds to CMD_STATUS (0x07), data is 15 bytes:
#define STAT_RUN        0   // Run state: 0x0A=running, 0x04=stopped
#define STAT_MODE       1   // Operating mode (see MODE_* constants)
#define STAT_DRIVE      2   // Drive state (0x02 = ready/OK)
#define STAT_PWR_HI     3   // Power consumption high byte (watts)
#define STAT_PWR_LO     4   // Power consumption low byte
#define STAT_RPM_HI     5   // Speed high byte (RPM)
#define STAT_RPM_LO     6   // Speed low byte
#define STAT_GPM        7   // Flow rate (GPM)
#define STAT_PPC        8   // PPC / chlorinator level
#define STAT_BYTE_9     9   // Unknown
#define STAT_ERR        10  // Error code (0x00 = no error)
#define STAT_BYTE_11    11  // Unknown
#define STAT_TIMER      12  // Timer remaining (minutes)
#define STAT_CLK_HOUR   13  // Clock: hour
#define STAT_CLK_MIN    14  // Clock: minute
#define STAT_DATA_LEN   15  // Total status data length

// Drive states
#define DRIVE_READY     0x02

// =============================================
// TIMING CONSTANTS
// =============================================
// External program commands must be re-sent every 30 seconds
// or the pump will stop executing the program and halt
#define EXT_PROG_REPEAT_INTERVAL  30000  // 30 seconds in ms

// Command timeout - how long to wait for pump response
#define CMD_RESPONSE_TIMEOUT      2000   // 2 seconds in ms

// Status query interval
#define STATUS_QUERY_INTERVAL     15000  // 15 seconds in ms

// =============================================
// PROTOCOL HELPER FUNCTIONS
// =============================================

/*
 * Calculate Pentair checksum.
 * Sum of all bytes from A5 (index 3) through end of data.
 * Returns 16-bit checksum.
 */
inline uint16_t pentairChecksum(const uint8_t* packet, size_t length) {
  uint16_t sum = 0;
  // Start from PKT_IDX_LEAD (0xA5) to end of data (length - 2 for checksum bytes)
  for (size_t i = PKT_IDX_LEAD; i < length - 2; i++) {
    sum += packet[i];
  }
  return sum;
}

/*
 * Verify a received packet's checksum.
 * Returns true if valid.
 */
inline bool pentairVerifyChecksum(const uint8_t* packet, size_t length) {
  if (length < PENTAIR_MIN_PKT_LEN) return false;
  
  uint16_t calculated = pentairChecksum(packet, length);
  uint16_t received = ((uint16_t)packet[length - 2] << 8) | packet[length - 1];
  return (calculated == received);
}

/*
 * Find a Pentair message in a buffer.
 * Looks for the FF 00 FF A5 preamble.
 * Returns the index of the 0xFF start, or -1 if not found.
 */
inline int pentairFindMessage(const uint8_t* buffer, size_t length) {
  if (length < PENTAIR_MIN_PKT_LEN) return -1;
  
  for (size_t i = 0; i <= length - 4; i++) {
    if (buffer[i]   == 0xFF && 
        buffer[i+1] == 0x00 && 
        buffer[i+2] == 0xFF && 
        buffer[i+3] == 0xA5) {
      return (int)i;
    }
  }
  return -1;
}

/*
 * Get the total packet length given the data at msgStart.
 * Returns total bytes from preamble through checksum.
 */
inline size_t pentairPacketLength(const uint8_t* packetStart) {
  // preamble(4) + ver(1) + dst(1) + src(1) + cmd(1) + len(1) + data(n) + checksum(2)
  return 4 + 1 + 1 + 1 + 1 + 1 + packetStart[PKT_IDX_LEN] + 2;
}

/*
 * Build a Pentair packet.
 * Fills in preamble, version, addresses, command, data, and checksum.
 * Returns total packet length.
 * 
 * Buffer must be large enough: 11 + dataLen bytes minimum.
 */
inline size_t pentairBuildPacket(uint8_t* buffer, uint8_t dst, uint8_t src,
                                 uint8_t cmd, const uint8_t* data, uint8_t dataLen) {
  size_t idx = 0;
  
  // Preamble
  buffer[idx++] = 0xFF;
  buffer[idx++] = 0x00;
  buffer[idx++] = 0xFF;
  buffer[idx++] = 0xA5;
  
  // Header
  buffer[idx++] = PENTAIR_VERSION;  // Version byte
  buffer[idx++] = dst;              // Destination
  buffer[idx++] = src;              // Source
  buffer[idx++] = cmd;              // Command/CFI
  buffer[idx++] = dataLen;          // Data length
  
  // Data
  for (uint8_t i = 0; i < dataLen; i++) {
    buffer[idx++] = data[i];
  }
  
  // Checksum (from A5 through end of data)
  uint16_t checksum = 0;
  for (size_t i = PKT_IDX_LEAD; i < idx; i++) {
    checksum += buffer[i];
  }
  buffer[idx++] = (checksum >> 8) & 0xFF;  // Checksum high byte
  buffer[idx++] = checksum & 0xFF;         // Checksum low byte
  
  return idx;
}

#endif // PENTAIR_PROTOCOL_H
