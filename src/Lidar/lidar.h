#ifndef LIDAR_H
#define LIDAR_H
#include <Arduino.h>
#include <Wire.h>

#include <Adafruit_VL53L0X.h>  

class Lidar
{
public:
    Lidar();
    ~Lidar();
    bool begin(TwoWire *bus,int _sda,int _scl);
    bool start();
    void stop();
    bool readDisFlux(int16_t &distance, int16_t &flux);

private:
    // To track number of instances
    uint8_t instanceId; // Unique ID for this instance
    char taskName[16];
    Adafruit_VL53L0X sensor = Adafruit_VL53L0X();
    const double f_s = 260; // Hz
    bool _initialized = false;
    TwoWire *_bus;
    uint8_t _sda;
    uint8_t _scl;
    bool _running;
    int16_t _distance;
    int16_t _flux;
    static uint8_t instanceCounter;
    static void readTask(void *pvParameters);
    SemaphoreHandle_t mutex;
};

#endif