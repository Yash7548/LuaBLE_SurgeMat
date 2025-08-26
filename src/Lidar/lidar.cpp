#include "lidar.h"

uint8_t Lidar::instanceCounter = 0;

Lidar::Lidar()
   
{
    instanceId = ++instanceCounter;
    _running = false;
    _distance = 0;

    // Initialize the mutex for this instance
    mutex = xSemaphoreCreateMutex();
}

bool Lidar::begin(TwoWire *_bus,int _sda,int _scl)
{

    // _bus->begin(_sda, _scl);
    sensor.begin(0x29, true, _bus);
    return (mutex != NULL); // Ensure mutex is successfully created
}

bool Lidar::start()
{
    if (!_running)
    {
        sensor.startRangeContinuous();
        _running = true;

        // Use a unique task name for each instance
        snprintf(taskName, sizeof(taskName), "LidarTask%d", instanceId);
        
        BaseType_t result = xTaskCreate(
            readTask, // Task function
            taskName, // Task name
            3000,     // Stack size
            this,     // Parameters
            1,        // Priority
            NULL      // Task handle
        );

        return (result == pdPASS);
    }
    return false;
}

void Lidar::stop()
{
    _running = false;
}

void Lidar::readTask(void *pvParameters)
{
    Lidar *lidar = static_cast<Lidar *>(pvParameters);

    while (lidar->_running)
    {
        

        // Lock the mutex before accessing shared variables
        if (xSemaphoreTake(lidar->mutex, portMAX_DELAY) == pdTRUE)
        {
            int16_t distance = lidar->sensor.readRange();
            lidar->_distance = distance;
            // Here you could add code to read flux if applicable
            xSemaphoreGive(lidar->mutex); // Release the mutex
        }

        vTaskDelay(33); // Delay for next reading
    }
    vTaskDelete(NULL); // Delete the task when done
}

bool Lidar::readDisFlux(int16_t &distance, int16_t &flux)
{
    if (_running)
    {
        // Lock the mutex before accessing shared variables
        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
        {
            distance = _distance;
            flux = _flux;
            xSemaphoreGive(mutex); // Release the mutex
        }
        return true;
    }
    return false;
}

Lidar::~Lidar()
{
    if (mutex != NULL) {
        vSemaphoreDelete(mutex);  // Delete the mutex when the object is destroyed
    }
}
