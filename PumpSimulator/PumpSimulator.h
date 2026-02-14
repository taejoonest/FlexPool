/*
 * Pump Simulator Class
 * Simulates a Pentair pump responding to speed commands
 */

#ifndef PUMPSIMULATOR_H
#define PUMPSIMULATOR_H

#include <Arduino.h>
#include <cstdint>
#include <cstddef>

class PumpSimulator {
private:
  uint16_t current_speed_rpm_;      // Current pump speed (0-3450 RPM)
  uint16_t target_speed_rpm_;       // Target speed
  bool is_running_;
  uint8_t pump_address_;            // Pump address on RS-485 bus
  unsigned long last_update_;
  
  // Simple protocol simulation
  // Real Pentair protocol is more complex, this is simplified
  size_t createStatusResponse(uint8_t* buffer);
  size_t createSpeedResponse(uint8_t* buffer);
  
public:
  PumpSimulator() 
    : current_speed_rpm_(0), target_speed_rpm_(0), is_running_(false),
      pump_address_(0x10), last_update_(0) {
  }
  
  void begin() {
    current_speed_rpm_ = 0;
    target_speed_rpm_ = 0;
    is_running_ = false;
    last_update_ = millis();
  }
  
  // Process incoming command and generate response
  size_t processCommand(const uint8_t* command, size_t length, uint8_t* response);
  
  // Update pump state (gradual speed changes)
  void update();
  
  uint16_t getCurrentSpeed() const { return current_speed_rpm_; }
  uint16_t getTargetSpeed() const { return target_speed_rpm_; }
  bool isRunning() const { return is_running_; }
};

// Implementation in header for Arduino IDE simplicity
size_t PumpSimulator::processCommand(const uint8_t* command, size_t length, uint8_t* response) {
  if (command == nullptr || length < 3) return 0;
  
  // Simplified protocol simulation
  // Real Pentair uses specific command structure
  // This is a basic example that responds to speed commands
  
  // Example: Simple speed command format
  // [Address] [Command] [Speed High] [Speed Low] [Checksum]
  // For demo: Accept any 3+ byte command and respond
  
  if (length >= 3) {
    // Check if it's a speed command (simplified)
    // In real protocol, you'd parse the actual Pentair command structure
    
    // Example: If command[1] is 0x01, treat as speed set command
    if (length >= 5 && command[1] == 0x01) {
      // Extract speed from bytes 2 and 3
      target_speed_rpm_ = (command[2] << 8) | command[3];
      target_speed_rpm_ = constrain(target_speed_rpm_, 0, 3450); // Max 3450 RPM
      is_running_ = (target_speed_rpm_ > 0);
      
      Serial.printf("Setting pump speed to: %d RPM\n", target_speed_rpm_);
      
      // Create response
      return createSpeedResponse(response);
    }
    // Status request
    else if (command[1] == 0x02) {
      Serial.println("Status request received");
      return createStatusResponse(response);
    }
    // Echo back for testing
    else {
      // Echo command back as response (for testing)
      for (size_t i = 0; i < length && i < 256; i++) {
        response[i] = command[i];
      }
      return length;
    }
  }
  
  return 0;
}

size_t PumpSimulator::createStatusResponse(uint8_t* buffer) {
  // Create status response
  // Format: [Address] [Status] [Speed High] [Speed Low] [Running] [Checksum]
  buffer[0] = pump_address_;
  buffer[1] = 0x82; // Status response
  buffer[2] = (current_speed_rpm_ >> 8) & 0xFF;
  buffer[3] = current_speed_rpm_ & 0xFF;
  buffer[4] = is_running_ ? 0x01 : 0x00;
  buffer[5] = 0x00; // Simple checksum (would be calculated in real protocol)
  return 6;
}

size_t PumpSimulator::createSpeedResponse(uint8_t* buffer) {
  // Acknowledge speed change
  buffer[0] = pump_address_;
  buffer[1] = 0x81; // Speed set acknowledge
  buffer[2] = (target_speed_rpm_ >> 8) & 0xFF;
  buffer[3] = target_speed_rpm_ & 0xFF;
  buffer[4] = 0x00; // Checksum
  return 5;
}

void PumpSimulator::update() {
  unsigned long now = millis();
  
  // Gradually change speed towards target (simulate pump acceleration)
  if (now - last_update_ >= 100) { // Update every 100ms
    if (current_speed_rpm_ < target_speed_rpm_) {
      current_speed_rpm_ += 50; // Increase by 50 RPM per update
      if (current_speed_rpm_ > target_speed_rpm_) {
        current_speed_rpm_ = target_speed_rpm_;
      }
    } else if (current_speed_rpm_ > target_speed_rpm_) {
      current_speed_rpm_ -= 50; // Decrease by 50 RPM per update
      if (current_speed_rpm_ < target_speed_rpm_) {
        current_speed_rpm_ = target_speed_rpm_;
      }
    }
    
    if (current_speed_rpm_ == 0) {
      is_running_ = false;
    }
    
    last_update_ = now;
  }
}

#endif
