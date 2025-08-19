#include "ble_handlers.h"
extern "C"
{
#define LUAT_LOG_TAG "STORAGE_CTRL"
#include "luat_log.h"
}

void initializeBLEHandlers()
{



}

/*********************************BLE & OTA HANDLERS *******************************************/

void onBLEConnect()
{
    LLOGI("BLE Connected!");
}

void onBLEDisconnect()
{
    LLOGI("BLE Disconnected!");

}

void onOtaStart()
{
    LLOGI("OTA update started");
    JsonDocument responce;
    responce["msgtyp"] = "otastart";
    bleController.sendMessage(responce);
}
void onOtaProgress(int percentage)
{
    Serial.printf("OTA progress: %d%%\n", percentage);
}

void onOtaSuccess()
{
    LLOGI("OTA update successful, restarting...");
    // buzzer.beepAsync(1500, 0, 1);
    JsonDocument responce;
    responce["msgtyp"] = "ota";
    responce["result"] = 1;
    bleController.sendMessage(responce);
}

void onOtaError(const char *error)
{
    Serial.printf("OTA error: %s\n", error);
    // buzzer.beepAsync(500, 200, 3);
    JsonDocument responce;
    responce["msgtyp"] = "ota";
    responce["result"] = 0;
    responce["error"] = error;
    bleController.sendMessage(responce);
}





void processBLEMessages()
{
    bleController.processJsonMessages();
    bleController.processTextMessages();
}
