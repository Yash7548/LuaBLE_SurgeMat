/*
 * ForceSensor.h - Minimal library for force sensor reading with ESP32
 * Created by Claude, May 9, 2025
 * Modified to support multiple force sensors
 */

#ifndef ForceSensor_h
#define ForceSensor_h

#include "Arduino.h"

class ForceSensor {
  private:
    uint8_t _pin;                // Analog input pin
    int _samples;                // Number of samples for averaging
    float _filterAlpha;          // Low pass filter alpha (0-1)
    float _filteredValue;        // Filtered sensor value
    
  public:
    // Constructor
    ForceSensor(uint8_t pin, int samples = 5, float filterAlpha = 0.2);
    
    // Methods
    void begin();
    int read();
    float readAverage();
    float readFiltered();
    float readMapped(float fromLow, float fromHigh, float toLow, float toHigh);
    float map(float x, float in_min, float in_max, float out_min, float out_max);
};

#endif