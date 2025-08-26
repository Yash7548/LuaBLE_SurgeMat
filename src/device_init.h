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
void initializeLidar();
void initializeForceSensor();


#endif // DEVICE_INIT_H