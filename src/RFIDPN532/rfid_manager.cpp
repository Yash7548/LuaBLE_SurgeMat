#include "rfid_manager.h"

void RFIDReader::readerTaskWrapper(void* args) {
    RFIDReader* reader = static_cast<RFIDReader*>(args);
    reader->readerTask();
}

void RFIDReader::readerTask() {
    uint8_t success = NULL;
    uint8_t uid[8] = {};  // Buffer to store the returned UID
    uint8_t uidLength;  // Length of the UID
            Serial.println("task");
    while(1) {
        if (!isRunning) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
         nfc->begin();
        success = nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength,2000);
        Serial.printf("fucker %d",uidLength);
        if (success) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            if (lastReadData == nullptr) {    
                lastReadData=0;                                                                      
                lastReadData = new uint8_t[uidLength];
                memcpy(lastReadData, uid, uidLength);
                lastReadLength = uidLength;
            }
            xSemaphoreGive(dataMutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));  // 100ms delay between reads
    }
}

RFIDReader::RFIDReader() {
    nfc = nullptr;
    isRunning = false;
    lastReadData = nullptr;
    lastReadLength = 0;
    dataMutex = xSemaphoreCreateMutex();
    readerTaskHandle = nullptr;
}

RFIDReader::~RFIDReader() {
    if (readerTaskHandle != nullptr) {
        vTaskDelete(readerTaskHandle);
    }
    if (lastReadData != nullptr) {
        delete[] lastReadData;
    }
    vSemaphoreDelete(dataMutex);
}

bool RFIDReader::begin(Adafruit_PN532& nfc_instance) {
    nfc = &nfc_instance;
    
    // Check if PN532 is responding
    nfc->begin();
    uint32_t versiondata = nfc->getFirmwareVersion();
    if (!versiondata) {
        nfc = nullptr;
        return false;  // PN532 not found
    }
      nfc->setPassiveActivationRetries(0x01);

    // Configure PN532
    nfc->SAMConfig();
    
    // Create RTOS task
    BaseType_t ret = xTaskCreate(
        readerTaskWrapper,
        "RFID_Reader",
        2048,        // Stack size
        this,        // Task parameters
        5,          // Priority
        &readerTaskHandle
    );
    
    if (ret != pdPASS) {
        nfc = nullptr;
        return false;
    }
    
    return true;
}

void RFIDReader::startReading() {
    isRunning = true;
}

void RFIDReader::stopReading() {
    isRunning = false;
}

bool RFIDReader::readData(uint8_t* buffer, uint8_t* length) {
    bool hasData = false;
    
    xSemaphoreTake(dataMutex, portMAX_DELAY);

delay(10);
    if (lastReadData != nullptr) {
        memcpy(buffer, lastReadData, lastReadLength);
        *length = lastReadLength;
        delete[] lastReadData;
        lastReadData = nullptr;
        hasData = true;
    }
    xSemaphoreGive(dataMutex);
    
    return hasData;
}

bool RFIDReader::isReading() {
    return isRunning;
}