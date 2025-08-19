#ifndef BLE_HANDLERS_H
#define BLE_HANDLERS_H

#include <ArduinoJson.h>

// #include "motor/motion_system.h"
#include "Global/global.h"

void initializeBLEHandlers();
void processBLEMessages();
void onBLEConnect();
void onBLEDisconnect();
void onOtaStart();
void onOtaProgress(int percentage);
void onOtaSuccess();
void onOtaError(const char *error);

// Add more handler declarations as needed

#endif // BLE_HANDLERS_H