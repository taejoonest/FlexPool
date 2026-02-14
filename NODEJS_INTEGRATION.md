# How nodejs-poolController Works with ESP32

## Important Clarification

**I did NOT use nodejs-poolController's code to write the ESP32 firmware.**

Instead, I designed the ESP32 code to be **compatible** with how nodejs-poolController communicates.

## How nodejs-poolController Actually Works

### Normal Setup (Without ESP32)

```
┌─────────────────────┐
│  Computer/Raspberry Pi│
│  nodejs-poolController│
│  (Node.js app)       │
└──────────┬───────────┘
           │ RS-485 Serial Port
           │ (Direct connection)
           ↓
┌─────────────────────┐
│  Pentair Controller  │
│  (EasyTouch,         │
│   IntelliCenter)     │
└─────────────────────┘
```

**nodejs-poolController:**
- Runs on a computer/Raspberry Pi
- Connects directly to Pentair equipment via RS-485 serial port
- Sends/receives Pentair protocol commands
- Provides web interface, MQTT, API, etc.

### With ESP32 Bridge (What We're Building)

```
┌─────────────────────┐
│  Computer            │
│  nodejs-poolController│
│  (Node.js app)       │
└──────────┬───────────┘
           │ USB Serial
           │ (COM port)
           ↓
┌─────────────────────┐
│  Controller ESP32    │
│  (Our code)          │
│  - Receives from USB │
│  - Forwards to RS-485│
└──────────┬───────────┘
           │ RS-485
           ↓
┌─────────────────────┐
│  Pump/Pump Simulator │
└─────────────────────┘
```

## What nodejs-poolController Sends

When nodejs-poolController communicates, it sends **raw RS-485 bytes** through a serial port.

### In nodejs-poolController's config.json:

```json
{
  "controller": {
    "rs485Port": "COM3",  // Windows
    // or "/dev/ttyUSB0" on Linux
    // or "/dev/cu.usbserial-xxx" on Mac
    "portSettings": {
      "baudRate": 9600
    }
  }
}
```

### What It Actually Sends

nodejs-poolController sends **binary data** (bytes) over the serial port, not hex strings.

For example, to set pump speed, it might send:
```
[0x10, 0x01, 0x02, 0x34, 0x56]
```

These are **raw bytes**, not a hex string.

## The Problem with My Current Code

My current `PumpController.ino` expects **hex strings** like `"1001023456"`, but nodejs-poolController sends **raw binary bytes**.

## Solution: Two Options

### Option 1: Binary Mode (Correct for nodejs-poolController)

Modify `PumpController.ino` to handle **raw binary** instead of hex strings:

```cpp
void loop() {
  // Check for binary data from USB Serial
  if (Serial.available()) {
    // Read raw bytes (not hex string)
    size_t len = Serial.readBytes(txBuffer, 256);
    
    if (len > 0) {
      // Send directly to RS-485 (no conversion needed)
      rs485.write(txBuffer, len);
      
      Serial.printf("Sent to pump: %d bytes\n", len);
    }
  }
  
  // Check for responses from pump
  if (rs485.available()) {
    size_t len = rs485.readBytes(rxBuffer, 256);
    if (len > 0) {
      // Forward raw bytes to Serial (no conversion)
      Serial.write(rxBuffer, len);
    }
  }
}
```

### Option 2: Keep Hex Mode (For Manual Testing)

Keep the current hex string mode for manual testing via Serial Monitor, but add binary mode support.

## How to Actually Use nodejs-poolController

### Step 1: Install nodejs-poolController

```bash
git clone https://github.com/tagyoureit/nodejs-poolController.git
cd nodejs-poolController
npm install
```

### Step 2: Configure for ESP32

In `config.json`:

```json
{
  "controller": {
    "rs485Port": "COM3",  // Your Controller ESP32's COM port
    "portSettings": {
      "baudRate": 115200,  // USB Serial baud rate
      "dataBits": 8,
      "stopBits": 1,
      "parity": "none"
    }
  }
}
```

### Step 3: ESP32 Code Must Handle Binary

The ESP32 needs to:
1. Receive **binary bytes** from Serial (not hex strings)
2. Forward **binary bytes** to RS-485
3. Receive **binary bytes** from RS-485
4. Forward **binary bytes** to Serial

## Updated Code for nodejs-poolController

I should update `PumpController.ino` to handle binary communication properly. Would you like me to:

1. **Update the code** to handle binary (raw bytes) instead of hex strings?
2. **Add both modes** - binary for nodejs-poolController, hex for manual testing?
3. **Show the exact protocol** that nodejs-poolController uses?

## Summary

- **nodejs-poolController** = Node.js app that runs on computer
- **ESP32 code** = Arduino firmware I wrote to bridge USB Serial ↔ RS-485
- **Connection** = nodejs-poolController sends binary bytes via USB Serial to ESP32
- **Current issue** = My code expects hex strings, but nodejs-poolController sends binary

Let me know if you want me to fix the code to work properly with nodejs-poolController!
