#ifndef RFID_READER_H
#define RFID_READER_H

#include <SPI.h>
#include <Adafruit_PN532.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

class RFIDReader {
private:
    Adafruit_PN532* nfc;
    TaskHandle_t readerTaskHandle;
    SemaphoreHandle_t dataMutex;
    volatile bool isRunning;
    uint8_t* lastReadData;
    uint8_t lastReadLength;
    
    
    static void readerTaskWrapper(void* args);
    void readerTask();

public:
    RFIDReader();
    ~RFIDReader();
    
    bool begin(Adafruit_PN532& nfc_instance);
    void startReading();
    void stopReading();
    bool readData(uint8_t* buffer, uint8_t* length);
    bool isReading();
};

#endif // RFID_READER_H