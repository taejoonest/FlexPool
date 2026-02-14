/*
 * FlexPool ESP32 RS-485 Interface
 * 
 * Main entry point for PlatformIO
 * 
 * Hardware Requirements:
 * - ESP32 development board
 * - MAX485 or similar RS-485 transceiver module
 * - RS-485 pool equipment (Pentair, etc.)
 */

#include <Arduino.h>
#include "include/Application.h"

// Global application instance
Application app;

void setup() {
    if (!app.initialize()) {
        // Halt on initialization failure
        while (true) {
            delay(1000);
        }
    }
}

void loop() {
    app.run();
}
