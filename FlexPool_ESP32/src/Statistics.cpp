/*
 * Statistics Class Implementation
 * 
 * Tracks RS-485 communication statistics
 */

#include "../include/Statistics.h"
#include <Arduino.h>

Statistics::Statistics() 
    : packets_received_(0), 
      packets_transmitted_(0), 
      errors_(0),
      start_time_(millis()) {
}

void Statistics::incrementReceived() {
    packets_received_++;
}

void Statistics::incrementTransmitted() {
    packets_transmitted_++;
}

void Statistics::incrementErrors() {
    errors_++;
}

void Statistics::reset() {
    packets_received_ = 0;
    packets_transmitted_ = 0;
    errors_ = 0;
    start_time_ = millis();
}

unsigned long Statistics::getPacketsReceived() const {
    return packets_received_;
}

unsigned long Statistics::getPacketsTransmitted() const {
    return packets_transmitted_;
}

unsigned long Statistics::getErrors() const {
    return errors_;
}

unsigned long Statistics::getUptimeSeconds() const {
    return (millis() - start_time_) / 1000;
}

void Statistics::print(Print& output) const {
    output.println("\n--- Statistics ---");
    output.printf("Packets Received: %lu\n", packets_received_);
    output.printf("Packets Transmitted: %lu\n", packets_transmitted_);
    output.printf("Errors: %lu\n", errors_);
    output.printf("Uptime: %lu seconds\n", getUptimeSeconds());
    output.println("------------------\n");
}
