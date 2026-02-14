/*
 * Statistics Class Header
 * 
 * Tracks RS-485 communication statistics
 */

#ifndef STATISTICS_H
#define STATISTICS_H

#include <Arduino.h>
#include <cstdint>

class Statistics {
public:
    Statistics();
    
    void incrementReceived();
    void incrementTransmitted();
    void incrementErrors();
    void reset();
    
    unsigned long getPacketsReceived() const;
    unsigned long getPacketsTransmitted() const;
    unsigned long getErrors() const;
    unsigned long getUptimeSeconds() const;
    
    void print(Print& output) const;

private:
    unsigned long packets_received_;
    unsigned long packets_transmitted_;
    unsigned long errors_;
    unsigned long start_time_;
};

#endif // STATISTICS_H
