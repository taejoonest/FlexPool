# Pump Controller ESP32

This ESP32 acts as a bridge between your computer (running nodejs-poolController) and the pump on the RS-485 bus.

## Setup

1. **Hardware:**
   - ESP32 board
   - MAX485 RS-485 transceiver module
   - USB-C cable to computer

2. **Wiring:**
   ```
   ESP32          MAX485 Module
   GPIO 17  ----> DI (Data In)
   GPIO 16  <---- RO (Receive Out)
   GPIO 4   ----> DE & RE (Drive/Receive Enable)
   GND      ----> GND
   3.3V     ----> VCC
   
   MAX485 A/B  <--->  Pump Simulator ESP32 (RS-485 A/B)
   ```

3. **Upload:**
   - Open `PumpController.ino` in Arduino IDE
   - Select your ESP32 board
   - Upload

4. **Usage:**
   - Connect to computer via USB-C
   - nodejs-poolController sends hex commands via Serial
   - Controller forwards commands to RS-485 bus
   - Responses from pump are forwarded back to Serial

## Communication Flow

```
Computer (nodejs-poolController)
    ↓ USB Serial (hex commands)
Controller ESP32
    ↓ RS-485
Pump Simulator ESP32
```

## Testing

Open Serial Monitor (115200 baud) and send hex commands:
- Example: `1001023456` (sets speed to 0x0234 = 564 RPM)
