/*
 * ForceSensor.cpp - Implementation of force sensor library
 * Created by Claude, May 9, 2025
 */

#include "ForceSensor.h"

// Constructor - matches the single constructor in the updated header
ForceSensor::ForceSensor(uint8_t pin, int samples, float filterAlpha) {
  _pin = pin;
  _samples = samples;
  _filterAlpha = filterAlpha;
  _filteredValue = 0;
}

// Initialize the sensor
void ForceSensor::begin() {
  pinMode(_pin, INPUT);
  
  // Take initial readings to stabilize
  for (int i = 0; i < 3; i++) {
    read();
    delay(5);
  }
  
  _filteredValue = readAverage();
}

// Read raw value
int ForceSensor::read() {
  return analogRead(_pin);
}

// Read average of multiple samples
float ForceSensor::readAverage() {
  float sum = 0;
  for (int i = 0; i < _samples; i++) {
    sum += read();
    delay(2);
  }
  return sum / _samples;
}

// Read with low-pass filter for stability
float ForceSensor::readFiltered() {
  float currentReading = readAverage();
  _filteredValue = _filterAlpha * currentReading + (1 - _filterAlpha) * _filteredValue;
  return _filteredValue;
}

// Map sensor reading to a custom range
float ForceSensor::readMapped(float fromLow, float fromHigh, float toLow, float toHigh) {
  float reading = readFiltered();
  return map(reading, fromLow, fromHigh, toLow, toHigh);
}

// Arduino's map() function but for floating point
float ForceSensor::map(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}