# Pump Simulator ESP32

This ESP32 simulates a Pentair pump on the RS-485 bus for testing.

## Setup

1. **Hardware:**
   - ESP32 board
   - MAX485 RS-485 transceiver module
   - USB-C cable (optional, for debugging)

2. **Wiring:**
   ```
   ESP32          MAX485 Module
   GPIO 17  ----> DI (Data In)
   GPIO 16  <---- RO (Receive Out)
   GPIO 4   ----> DE & RE (Drive/Receive Enable)
   GND      ----> GND
   3.3V     ----> VCC
   
   MAX485 A/B  <--->  Controller ESP32 (RS-485 A/B)
   ```

3. **Upload:**
   - Open `PumpSimulator.ino` in Arduino IDE
   - Select your ESP32 board
   - Upload

4. **Usage:**
   - Receives speed commands from Controller ESP32
   - Responds with pump status
   - Simulates gradual speed changes

## Protocol (Simplified)

This is a **simplified simulation**. Real Pentair protocol is more complex.

**Speed Set Command:**
- Format: `[Address] [0x01] [Speed High] [Speed Low] [Checksum]`
- Example: `1001023456` sets speed to 0x0234 = 564 RPM

**Status Request:**
- Format: `[Address] [0x02] [Checksum]`
- Response: `[Address] [0x82] [Speed High] [Speed Low] [Running] [Checksum]`

## Testing

Connect Serial Monitor (115200 baud) to see:
- Received commands
- Sent responses
- Current pump speed
