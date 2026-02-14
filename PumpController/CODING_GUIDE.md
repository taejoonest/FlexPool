# How to Code PumpController.ino - Step by Step Guide

## Overview

`PumpController.ino` is a **bridge** that:
- Receives commands from your computer (via USB Serial)
- Forwards them to the pump (via RS-485)
- Receives responses from the pump
- Sends responses back to the computer

## Code Structure

### 1. Includes and Setup

```cpp
#include "RS485Simple.h"  // Our helper library for RS-485
```

This includes the RS-485 library that handles the low-level communication.

### 2. Configuration

```cpp
#define RS485_DE_RE_PIN 4
#define RS485_TX_PIN 17
#define RS485_RX_PIN 16
#define RS485_BAUD_RATE 9600
```

These define which pins to use and the communication speed.

### 3. Create Objects

```cpp
RS485Simple rs485(Serial2, RS485_DE_RE_PIN, RS485_TX_PIN, RS485_RX_PIN);
uint8_t rxBuffer[256];  // For receiving
uint8_t txBuffer[256];  // For sending
```

- `rs485`: Object that handles RS-485 communication
- `rxBuffer`: Stores data received from pump
- `txBuffer`: Stores data to send to pump

### 4. setup() Function

Runs **once** when ESP32 starts:

```cpp
void setup() {
  Serial.begin(115200);        // USB Serial to computer
  rs485.begin(9600);           // RS-485 to pump
  // Print startup messages
}
```

### 5. loop() Function

Runs **continuously**:

#### Part A: Receive from Computer

```cpp
if (Serial.available()) {
  String command = Serial.readStringUntil('\n');
  // Convert hex string "1001" to bytes [0x10, 0x01]
  // Send to pump via RS-485
}
```

**What happens:**
1. Check if data available on USB Serial
2. Read hex string (e.g., "1001023456")
3. Convert to bytes
4. Send to pump

#### Part B: Receive from Pump

```cpp
if (rs485.available()) {
  size_t len = rs485.readBytes(rxBuffer, 256);
  // Forward to computer via Serial
}
```

**What happens:**
1. Check if pump sent data
2. Read bytes from RS-485
3. Convert to hex string
4. Send to computer

## Key Concepts

### Hex String to Bytes Conversion

**Input:** `"1001023456"` (hex string from Serial)

**Process:**
```cpp
"10" → 0x10 (16 decimal)
"01" → 0x01 (1 decimal)
"02" → 0x02 (2 decimal)
"34" → 0x34 (52 decimal)
"56" → 0x56 (86 decimal)
```

**Output:** `[0x10, 0x01, 0x02, 0x34, 0x56]` (byte array)

**Code:**
```cpp
for (int i = 0; i < len; i++) {
  String byteStr = command.substring(i * 2, i * 2 + 2);
  txBuffer[i] = strtol(byteStr.c_str(), NULL, 16);
}
```

### Bytes to Hex String Conversion

**Input:** `[0x10, 0x01, 0x02]` (byte array from pump)

**Output:** `"10 01 02"` (hex string to Serial)

**Code:**
```cpp
for (size_t i = 0; i < len; i++) {
  Serial.printf("%02X ", rxBuffer[i]);
}
```

## Communication Flow

```
┌─────────────────┐
│  Computer       │
│  (nodejs-       │
│   poolController)│
└────────┬────────┘
         │ USB Serial
         │ "1001023456" (hex string)
         ↓
┌─────────────────┐
│  PumpController │
│  ESP32          │
│                 │
│  1. Receives    │
│  2. Converts    │
│  3. Sends       │
└────────┬────────┘
         │ RS-485
         │ [0x10, 0x01, ...] (bytes)
         ↓
┌─────────────────┐
│  Pump           │
│  (or Simulator) │
└─────────────────┘
```

## Common Modifications

### Change Baud Rate

```cpp
#define RS485_BAUD_RATE 19200  // Change from 9600 to 19200
```

### Change Pins

```cpp
#define RS485_DE_RE_PIN 5   // Use GPIO 5 instead of 4
#define RS485_TX_PIN 18     // Use GPIO 18 instead of 17
#define RS485_RX_PIN 19     // Use GPIO 19 instead of 16
```

### Add Error Handling

```cpp
if (rs485.write(txBuffer, len) != len) {
  Serial.println("ERROR: Failed to send to pump");
}
```

### Add Protocol Parsing

```cpp
// Parse specific commands
if (txBuffer[0] == 0x10 && txBuffer[1] == 0x01) {
  Serial.println("Speed command detected");
  uint16_t speed = (txBuffer[2] << 8) | txBuffer[3];
  Serial.printf("Setting speed to: %d RPM\n", speed);
}
```

## Testing

### Manual Test

1. Upload code to ESP32
2. Open Serial Monitor (115200 baud)
3. Type: `1001023456`
4. Press Enter
5. Should see: `Sent to pump: 10 01 02 34 56`

### With Pump Simulator

1. Upload PumpController to ESP32 #1
2. Upload PumpSimulator to ESP32 #2
3. Connect RS-485 between them
4. Send command from Serial Monitor
5. Should see response from pump

## Troubleshooting

### No Response from Pump

- Check RS-485 wiring (A/B lines)
- Verify baud rate matches
- Check DE/RE pin connection
- Ensure pump is powered

### Serial Monitor Shows Nothing

- Check baud rate (115200)
- Verify USB cable is data-capable
- Check COM port selection
- Try resetting ESP32

### Commands Not Working

- Verify hex format (no spaces, uppercase)
- Check command length
- Ensure RS-485 transceiver is working
- Check Serial Monitor on both ESP32s
