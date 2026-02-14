/*
 * FlexPool ESP32 Configuration
 * 
 * Configure RS-485 and system settings here
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// RS-485 Configuration
// ============================================

// Serial port for RS-485 (ESP32 has multiple UARTs)
// Use Serial2 for most ESP32 boards (GPIO 16/17)
#define RS485_SERIAL Serial2

// RS-485 Transceiver Control Pin (DE/RE)
// This pin controls transmit/receive mode
// HIGH = Transmit, LOW = Receive
#define RS485_DE_RE_PIN 4

// RS-485 TX Pin (to transceiver DI)
#define RS485_TX_PIN 17

// RS-485 RX Pin (from transceiver RO)
#define RS485_RX_PIN 16

// RS-485 Baud Rate
// Common rates: 9600, 19200, 38400, 57600, 115200
// Pool controllers often use 9600 or 19200
#define RS485_BAUD_RATE 9600

// RS-485 Buffer Size
#define RS485_BUFFER_SIZE 256

// RS-485 Timeout (milliseconds)
// Set to 0 to disable timeout warnings
#define RS485_TIMEOUT_MS 30000  // 30 seconds

// ============================================
// Debug and Feature Flags
// ============================================

// Enable debug output
#define DEBUG_MODE 1

// Echo received packets back (for testing)
#define ECHO_MODE 0

// Enable WiFi functionality
#define ENABLE_WIFI 0

// ============================================
// WiFi Configuration (if enabled)
// ============================================

#ifdef ENABLE_WIFI
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
#endif

// ============================================
// Statistics and Monitoring
// ============================================

// Statistics print interval (milliseconds)
#define STATS_INTERVAL_MS 60000  // 1 minute

// ============================================
// Advanced RS-485 Settings
// ============================================

// Pre-transmit delay (microseconds)
// Delay before enabling transmit mode
#define RS485_PRE_TX_DELAY_US 10

// Post-transmit delay (microseconds)
// Delay before switching back to receive mode
#define RS485_POST_TX_DELAY_US 10

// Receive timeout (milliseconds)
#define RS485_RX_TIMEOUT_MS 100

#endif // CONFIG_H
