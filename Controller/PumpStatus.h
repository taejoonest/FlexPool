/*
 * =============================================
 * PumpStatus.h - Shared pump status struct
 * =============================================
 * 
 * Shared between Controller.ino and MQTTHandler.h
 * so both can access the pump's current state.
 */

#ifndef PUMP_STATUS_H
#define PUMP_STATUS_H

#include <Arduino.h>

// Last known pump status (from CMD_STATUS responses)
struct PumpStatus {
  bool     valid    = false;  // Have we received at least one status?
  bool     running  = false;
  uint16_t rpm      = 0;
  uint16_t watts    = 0;
  uint8_t  gpm      = 0;
  uint8_t  mode     = 0;
  uint8_t  errCode  = 0;
  uint8_t  drive    = 0;
  uint8_t  timer    = 0;
  uint8_t  hour     = 0;
  uint8_t  minute   = 0;
  unsigned long lastUpdate = 0;
};

#endif // PUMP_STATUS_H
