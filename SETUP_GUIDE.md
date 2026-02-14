# Complete Setup Guide: ESP32 Pump Control with nodejs-poolController

## Overview

This setup uses **two ESP32 boards** to simulate the communication between nodejs-poolController and a Pentair pump:

1. **Controller ESP32** - Bridge between computer and RS-485 bus
2. **Pump Simulator ESP32** - Simulates the pump

## Hardware Setup

### Controller ESP32
- Connected to computer via **USB-C** (for Serial communication)
- Connected to Pump Simulator via **RS-485** (MAX485 module)

### Pump Simulator ESP32
- Connected to Controller ESP32 via **RS-485** (MAX485 module)
- Optional: USB for debugging

### RS-485 Wiring (Both ESP32s)
```
ESP32          MAX485 Module
GPIO 17  ----> DI
GPIO 16  <---- RO
GPIO 4   ----> DE & RE
GND      ----> GND
3.3V     ----> VCC

Connect MAX485 A/B lines between the two ESP32s
```

## Software Setup

### Step 1: Upload Controller ESP32

1. Open Arduino IDE
2. Open `PumpController/PumpController.ino`
3. Select: **Tools → Board → ESP32 Dev Module**
4. Select: **Tools → Port → (your Controller ESP32 port)**
5. Click **Upload**

### Step 2: Upload Pump Simulator ESP32

1. Open Arduino IDE (or new window)
2. Open `PumpSimulator/PumpSimulator.ino`
3. Select: **Tools → Board → ESP32 Dev Module**
4. Select: **Tools → Port → (your Pump Simulator ESP32 port)**
5. Click **Upload**

### Step 3: Configure nodejs-poolController

The nodejs-poolController needs to communicate with the Controller ESP32 via Serial.

In nodejs-poolController's `config.json`, set:
```json
{
  "controller": {
    "rs485Port": "COM3",  // Windows: COM port of Controller ESP32
                          // Linux: /dev/ttyUSB0 or /dev/ttyACM0
                          // Mac: /dev/cu.usbserial-xxx
    "portSettings": {
      "baudRate": 115200
    }
  }
}
```

## Testing

### Manual Test (Serial Monitor)

1. Open Serial Monitor for Controller ESP32 (115200 baud)
2. Send hex command: `1001023456`
   - This sets pump speed to 564 RPM (0x0234)
3. Check Pump Simulator Serial Monitor to see response

### With nodejs-poolController

1. Start nodejs-poolController
2. It will send commands to Controller ESP32 via Serial
3. Controller forwards to Pump Simulator via RS-485
4. Pump Simulator responds back through the chain

## Arduino IDE File Organization

### PumpController Folder
```
PumpController/
├── PumpController.ino    (Main file - must match folder name)
└── RS485Simple.h          (Helper library)
```

### PumpSimulator Folder
```
PumpSimulator/
├── PumpSimulator.ino     (Main file - must match folder name)
├── RS485Simple.h          (Helper library)
└── PumpSimulator.h        (Pump simulation logic)
```

**Key Points:**
- Main `.ino` file must be in a folder with the same name
- Additional `.h` and `.cpp` files go in the same folder
- Arduino IDE automatically compiles all files in the folder

## Communication Flow

```
┌─────────────────────┐
│  nodejs-poolController │
│  (on Computer)        │
└──────────┬────────────┘
           │ USB Serial (hex commands)
           ↓
┌─────────────────────┐
│  Controller ESP32    │
│  (USB-C connected)   │
└──────────┬────────────┘
           │ RS-485
           ↓
┌─────────────────────┐
│  Pump Simulator ESP32│
│  (simulates pump)     │
└─────────────────────┘
```

## Troubleshooting

1. **No communication:**
   - Check RS-485 wiring (A/B lines connected correctly)
   - Verify both ESP32s are powered
   - Check baud rates match (9600 for RS-485)

2. **Serial Monitor shows nothing:**
   - Verify correct COM port selected
   - Check baud rate (115200 for USB Serial)
   - Ensure ESP32 is powered

3. **Commands not working:**
   - Check hex format (no spaces, uppercase)
   - Verify RS-485 transceiver is working
   - Check Serial Monitor on both ESP32s

## Next Steps

Once this works, you can:
1. Replace Pump Simulator with real Pentair pump
2. Add more sophisticated protocol parsing
3. Add WiFi connectivity to Controller ESP32
4. Integrate with Home Assistant via MQTT
