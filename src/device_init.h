#ifndef DEVICE_INIT_H
#define DEVICE_INIT_H

#include <Arduino.h>
#include "Global/global.h"

// Main initialization function
void initializeDevice();

void initializeBLEController(String deviceName="");
void intializeStorage();
void initRfidDisplay();
void initializeUserButton();

#endif // DEVICE_INIT_H