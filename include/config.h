#ifndef CONFIG_H
#define CONFIG_H
#include <Arduino.h>

// BLe
#define BLE_PRODUCT_UUID "AE06"
#define BLE_NAME_PREFIX "SurgeMat"

// Buzzer
#define BUZZER_PIN 47    // Default GPIO pin for piezo speaker
#define BUZZER_CHANNEL 4 // Default LEDC channel


//SPi Display
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240
#define DISPLAY_ROTATION 0
#define DISPLAY_MOSI 40
#define DISPLAY_MISO 41
#define DISPLAY_SCK 42
#define DISPLAY_CS 03
#define DISPLAY_DC 05
#define DISPLAY_RST 06
#define DISPLAY_BL 36

//RFID
#define RFID_RST 38
#define RFID_IRQ 37
#define RFID_MOSI 40
#define RFID_MISO 41
#define RFID_SCK 42
#define RFID_CS 02


//user button
#define USER_BUTTON 17


//LIDAR top
#define LIDAR_TOP_SDA 13
#define LIDAR_TOP_SCL 14

//LIDAR bottom
#define LIDAR_BOTTOM_SDA 39
#define LIDAR_BOTTOM_SCL 12


//Force Sensor
#define FORCE_SENSOR_LEFT_PIN   11
#define FORCE_SENSOR_RIGHT_PIN  07





#endif