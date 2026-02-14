# PumpController.ino - Bidirectional Bridge

## What It Actually Does

PumpController.ino is a **BIDIRECTIONAL bridge** that forwards data **both ways**:

### Direction 1: Computer → ESP32 → Pump (Commands)
```
Computer sends command
    ↓ USB Serial
ESP32 receives
    ↓ RS-485
Pump receives command
```

### Direction 2: Pump → ESP32 → Computer (Responses)
```
Pump sends response
    ↓ RS-485
ESP32 receives
    ↓ USB Serial
Computer receives response
```

## The Code Does BOTH

### PART 1: Computer → Pump (Lines 73-91)
```cpp
if (Serial.available()) {  // Check USB Serial (from computer)
  // Read from computer
  size_t len = Serial.readBytes(txBuffer, 256);
  // Forward to pump
  rs485.write(txBuffer, len);
}
```

### PART 2: Pump → Computer (Lines 97-113)
```cpp
if (rs485.available()) {  // Check RS-485 (from pump)
  // Read from pump
  size_t len = rs485.readBytes(rxBuffer, 256);
  // Forward to computer
  Serial.write(rxBuffer, len);
}
```

## Complete Flow Example

### Setting Pump Speed:

1. **Computer** (nodejs-poolController) sends: `[0x10, 0x01, 0x02, 0x34]`
   - Goes through USB Serial
   
2. **ESP32** receives via USB Serial
   - Forwards to RS-485
   
3. **Pump** receives via RS-485
   - Processes command
   - Sets speed to 564 RPM
   
4. **Pump** sends response: `[0x10, 0x82, 0x02, 0x34, 0x01]`
   - Goes through RS-485
   
5. **ESP32** receives via RS-485
   - Forwards to USB Serial
   
6. **Computer** receives via USB Serial
   - nodejs-poolController processes response

## Why It's Bidirectional

- **Commands** need to go: Computer → Pump
- **Responses** need to go: Pump → Computer
- The ESP32 forwards **both directions** automatically

## Summary

PumpController.ino is **NOT** just ESP32 → Pump.

It's a **full bidirectional bridge**:
- ✅ Computer → Pump (commands)
- ✅ Pump → Computer (responses)

Both directions happen automatically in the `loop()` function.
