/*
 * Serial Command Handler Implementation
 * 
 * Processes commands from Serial interface
 */

#include "../include/SerialCommandHandler.h"
#include <Arduino.h>
#include <cstdlib>
#include <cstring>

SerialCommandHandler::SerialCommandHandler(RS485& rs485, Statistics& stats)
    : rs485_(rs485), stats_(stats) {
}

void SerialCommandHandler::processCommand(const String& command) {
    if (command.length() == 0) return;
    
    // Convert hex string to bytes and send
    if (command.startsWith("send:")) {
        handleSendCommand(command);
    }
    // Reset statistics
    else if (command == "reset") {
        stats_.reset();
        Serial.println("Statistics reset");
    }
    // Print help
    else if (command == "help") {
        printHelp();
    }
    else {
        Serial.println("Unknown command. Type 'help' for commands.");
    }
}

void SerialCommandHandler::handleSendCommand(const String& command) {
    String hexData = command.substring(5);
    hexData.trim();
    
    // Parse hex string
    int len = hexData.length() / 2;
    if (len > 0 && len <= RS485_BUFFER_SIZE) {
        uint8_t data[RS485_BUFFER_SIZE];
        
        for (int i = 0; i < len; i++) {
            String byteStr = hexData.substring(i * 2, i * 2 + 2);
            char* endPtr;
            unsigned long value = strtoul(byteStr.c_str(), &endPtr, 16);
            
            if (*endPtr != '\0') {
                Serial.println("ERROR: Invalid hex character in data");
                return;
            }
            
            data[i] = static_cast<uint8_t>(value);
        }
        
        if (rs485_.write(data, len) == len) {
            stats_.incrementTransmitted();
            Serial.printf("Sent %d bytes\n", len);
        } else {
            stats_.incrementErrors();
            Serial.println("ERROR: Failed to transmit RS-485 data");
        }
    } else {
        Serial.println("ERROR: Invalid hex data length");
    }
}

void SerialCommandHandler::printHelp() {
    Serial.println("\nAvailable Commands:");
    Serial.println("  send:HEX_DATA  - Send hex data over RS-485 (e.g., send:010203)");
    Serial.println("  reset          - Reset statistics");
    Serial.println("  help           - Show this help");
    Serial.println();
}
