/*
 * FlexPool ESP32 RS-485 Interface
 * 
 * Arduino IDE compatibility entry point
 * For PlatformIO, use src/main.cpp instead
 * 
 * Note: Arduino IDE requires all .cpp files to be in the same directory
 * or properly included. This file includes the necessary source files.
 */

#include "include/config.h"
#include "include/RS485.h"
#include "include/Statistics.h"
#include "include/SerialCommandHandler.h"
#include "include/Application.h"

// Include implementations (Arduino IDE needs this)
#include "src/RS485.cpp"
#include "src/Statistics.cpp"
#include "src/SerialCommandHandler.cpp"
#include "src/Application.cpp"

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
