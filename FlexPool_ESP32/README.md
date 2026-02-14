# FlexPool ESP32 RS-485 Interface

An ESP32-based RS-485 interface for pool controller communication. This project provides a bridge between RS-485 pool equipment (Pentair, IntelliFlo, etc.) and network/WiFi connectivity.

## Features

- ✅ RS-485 communication with proper DE/RE control
- ✅ Configurable baud rates and pin assignments
- ✅ Serial debugging interface
- ✅ Packet statistics and monitoring
- ✅ Hex data transmission via Serial commands
- 🔄 WiFi/Network forwarding (framework ready)
- 🔄 MQTT integration (ready to implement)
- 🔄 Web server interface (ready to implement)

## Hardware Requirements

### ESP32 Development Board
- ESP32 DevKit, ESP32-WROOM, or similar
- Any ESP32 board with accessible GPIO pins

### RS-485 Transceiver Module
- MAX485, SP485, or similar RS-485 transceiver
- Common modules: MAX485 TTL to RS-485 Converter

### Pool Equipment
- RS-485 compatible pool controller (Pentair IntelliCenter, EasyTouch, etc.)

## Wiring Diagram

```
ESP32                    MAX485 Module              Pool Equipment
------                   ------------              --------------
GPIO 17 (TX)    -------> DI (Data In)
GPIO 16 (RX)    <------- RO (Receive Out)
GPIO 4  (DE/RE) -------> DE & RE (Drive Enable / Receive Enable)
GND             -------> GND
5V/3.3V         -------> VCC

MAX485 Module:
A (RS-485 A+)   -------> A+ (to pool equipment)
B (RS-485 B-)   -------> B- (to pool equipment)
GND             -------> GND (common ground)
```

### Pin Connections

| ESP32 Pin | MAX485 Pin | Description |
|-----------|-----------|-------------|
| GPIO 17   | DI        | Transmit Data |
| GPIO 16   | RO        | Receive Data |
| GPIO 4    | DE & RE   | Transmit/Receive Control |
| GND       | GND       | Ground |
| 5V/3.3V   | VCC       | Power (check module voltage) |

**Note:** Some MAX485 modules combine DE and RE into a single pin. Connect GPIO 4 to both DE and RE on the module.

## Software Setup

### Prerequisites

**Option 1: PlatformIO (Recommended for C++ Development)**
1. **VS Code** with **PlatformIO IDE** extension
2. Or **PlatformIO CLI**

**Option 2: Arduino IDE**
1. **Arduino IDE** (1.8.19 or later)
2. **ESP32 Board Support**
   - File → Preferences → Additional Boards Manager URLs
   - Add: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools → Board → Boards Manager → Search "ESP32" → Install

### Installation

#### Using PlatformIO (Recommended)

1. Clone or download this repository
2. Open the project in VS Code with PlatformIO extension
3. Configure settings in `include/config.h`:
   - Adjust GPIO pins if needed
   - Set baud rate (typically 9600 or 19200 for pool controllers)
   - Enable/disable debug mode
4. Build and upload:
   ```bash
   pio run -t upload
   ```
5. Monitor serial output:
   ```bash
   pio device monitor
   ```

#### Using Arduino IDE

1. Clone or download this repository
2. Open `FlexPool_ESP32.ino` in Arduino IDE
3. Select your ESP32 board:
   - Tools → Board → ESP32 Arduino → Your board model
4. Configure settings in `include/config.h`:
   - Adjust GPIO pins if needed
   - Set baud rate (typically 9600 or 19200 for pool controllers)
   - Enable/disable debug mode
5. Upload to ESP32

### Project Structure

```
FlexPool_ESP32/
├── src/                    # C++ source files
│   ├── main.cpp           # PlatformIO entry point
│   ├── Application.cpp    # Main application logic
│   ├── RS485.cpp          # RS-485 communication
│   ├── Statistics.cpp     # Statistics tracking
│   └── SerialCommandHandler.cpp  # Command processing
├── include/                # C++ header files
│   ├── Application.h
│   ├── RS485.h
│   ├── Statistics.h
│   ├── SerialCommandHandler.h
│   └── config.h           # Configuration
├── FlexPool_ESP32.ino     # Arduino IDE entry point
├── platformio.ini         # PlatformIO configuration
└── README.md
```

### Configuration

Edit `config.h` to match your setup:

```cpp
// RS-485 Pins
#define RS485_DE_RE_PIN 4    // Change if using different pin
#define RS485_TX_PIN 17      // Change if using different pin
#define RS485_RX_PIN 16      // Change if using different pin

// Baud Rate (check your pool controller documentation)
#define RS485_BAUD_RATE 9600  // Common: 9600, 19200, 38400

// Enable debug output
#define DEBUG_MODE 1          // Set to 0 to disable
```

## Usage

### Serial Monitor Commands

Open Serial Monitor at 115200 baud to interact with the interface:

- `send:HEX_DATA` - Send hex data over RS-485
  - Example: `send:01020304` sends bytes 0x01, 0x02, 0x03, 0x04
- `reset` - Reset statistics counters
- `help` - Show available commands

### Example: Sending a Test Packet

```
send:0102030405
```

This will transmit the hex bytes `01 02 03 04 05` over RS-485.

### Monitoring

The system automatically prints statistics every minute:
- Packets received
- Packets transmitted
- Error count
- Uptime

With `DEBUG_MODE` enabled, all received and transmitted packets are printed in hex format.

## Integration with nodejs-poolController

This ESP32 interface can act as a bridge between RS-485 pool equipment and the [nodejs-poolController](https://github.com/tagyoureit/nodejs-poolController) application:

1. **Direct Connection**: Connect ESP32 to pool equipment via RS-485
2. **Network Bridge**: ESP32 forwards RS-485 data over WiFi/Network
3. **Serial Bridge**: ESP32 connects to computer via USB, nodejs-poolController reads from serial port

### Future Enhancements

- [ ] WiFi web server for configuration
- [ ] MQTT integration for Home Assistant
- [ ] OTA (Over-The-Air) updates
- [ ] Packet filtering and routing
- [ ] Multiple RS-485 bus support
- [ ] WebSocket real-time monitoring

## Troubleshooting

### No Data Received

1. **Check Wiring**: Verify all connections, especially A/B lines
2. **Baud Rate**: Ensure baud rate matches pool controller (try 9600, 19200)
3. **Termination**: Some RS-485 networks require 120Ω termination resistors
4. **Ground**: Ensure common ground between ESP32, MAX485, and pool equipment
5. **DE/RE Pin**: Verify GPIO 4 is connected to both DE and RE on MAX485

### Transmission Issues

1. **Power**: Ensure MAX485 module has adequate power (3.3V or 5V)
2. **Delays**: Adjust `RS485_PRE_TX_DELAY_US` and `RS485_POST_TX_DELAY_US` in config.h
3. **Buffer**: Check if data is being sent too fast

### Serial Monitor Shows Nothing

1. **Baud Rate**: Ensure Serial Monitor is set to 115200
2. **USB Cable**: Use a data-capable USB cable
3. **Port**: Select correct COM port in Arduino IDE

## License

This project is provided as-is for pool automation enthusiasts.

## Contributing

Feel free to submit issues, feature requests, or pull requests!

## References

- [nodejs-poolController](https://github.com/tagyoureit/nodejs-poolController)
- [ESP32 Arduino Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [MAX485 Datasheet](https://www.ti.com/lit/ds/symlink/max485.pdf)
