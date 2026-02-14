/*
 * Application Class Implementation
 * 
 * Main application logic for FlexPool ESP32 RS-485 Interface
 */

#include "../include/Application.h"
#include <Arduino.h>

Application::Application()
    : rs485_(RS485_SERIAL, RS485_DE_RE_PIN, RS485_TX_PIN, RS485_RX_PIN),
      command_handler_(rs485_, statistics_),
      last_activity_(0),
      last_stats_print_(0) {
}

bool Application::initialize() {
    // Initialize Serial for debugging
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n=================================");
    Serial.println("FlexPool ESP32 RS-485 Interface");
    Serial.println("=================================\n");
    
    // Initialize RS-485
    if (rs485_.begin(RS485_BAUD_RATE)) {
        Serial.println("RS-485 initialized successfully");
        Serial.printf("Baud Rate: %d\n", RS485_BAUD_RATE);
        Serial.printf("DE/RE Pin: %d\n", RS485_DE_RE_PIN);
        Serial.printf("TX Pin: %d\n", RS485_TX_PIN);
        Serial.printf("RX Pin: %d\n", RS485_RX_PIN);
    } else {
        Serial.println("ERROR: RS-485 initialization failed!");
        return false;
    }
    
    // Initialize WiFi if enabled
    #ifdef ENABLE_WIFI
    initWiFi();
    #endif
    
    Serial.println("\nSystem ready. Waiting for RS-485 data...\n");
    last_activity_ = millis();
    
    return true;
}

void Application::run() {
    // Check for incoming RS-485 data
    if (rs485_.available()) {
        size_t rxLength = rs485_.readBytes(rx_buffer_, RS485_BUFFER_SIZE);
        
        if (rxLength > 0) {
            statistics_.incrementReceived();
            last_activity_ = millis();
            
            // Process received packet
            processReceivedPacket(rx_buffer_, rxLength);
            
            // Print received data (if debug enabled)
            #ifdef DEBUG_MODE
            printHexData("RX", rx_buffer_, rxLength);
            #endif
        }
    }
    
    // Check for data from Serial (for testing/forwarding)
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        command_handler_.processCommand(command);
    }
    
    // Print statistics periodically
    if (millis() - last_stats_print_ > STATS_INTERVAL_MS) {
        statistics_.print(Serial);
        last_stats_print_ = millis();
    }
    
    // Handle timeout if no activity
    if (RS485_TIMEOUT_MS > 0 && (millis() - last_activity_ > RS485_TIMEOUT_MS)) {
        #ifdef DEBUG_MODE
        Serial.println("Warning: No RS-485 activity detected");
        #endif
        last_activity_ = millis(); // Reset to avoid spam
    }
    
    delay(1); // Small delay to prevent watchdog issues
}

void Application::processReceivedPacket(const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) return;
    
    // Example: Echo back (for testing)
    #ifdef ECHO_MODE
    sendRS485Data(data, length);
    #endif
    
    // Example: Forward to WiFi/Network
    #ifdef ENABLE_WIFI
    forwardToNetwork(data, length);
    #endif
}

void Application::sendRS485Data(const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) return;
    
    if (length > RS485_BUFFER_SIZE) {
        Serial.println("ERROR: Data too large to transmit");
        statistics_.incrementErrors();
        return;
    }
    
    if (rs485_.write(data, length) == length) {
        statistics_.incrementTransmitted();
        #ifdef DEBUG_MODE
        printHexData("TX", data, length);
        #endif
    } else {
        statistics_.incrementErrors();
        Serial.println("ERROR: Failed to transmit RS-485 data");
    }
}

void Application::printHexData(const char* prefix, const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) return;
    
    Serial.printf("[%s] %d bytes: ", prefix, length);
    for (size_t i = 0; i < length; i++) {
        Serial.printf("%02X ", data[i]);
    }
    Serial.println();
}

#ifdef ENABLE_WIFI
void Application::initWiFi() {
    Serial.println("Initializing WiFi...");
    // Add WiFi initialization code here
    // WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("WiFi initialized (not yet implemented)");
}

void Application::forwardToNetwork(const uint8_t* data, size_t length) {
    // Add network forwarding logic here
    // This could send data via WiFi, MQTT, HTTP, etc.
}
#endif
