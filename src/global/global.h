#ifndef GLOBAL_H
#define GLOBAL_H

#include "config.h"
#include "lua_setup/lua_setup.h"
#include <esp_system.h>  // for esp_read_mac and ESP_MAC_BT

#include <BLEController.h>
#include "Storage/storage.h"
#include "ble_handlers.h"
#include "LuaQueue/lqueue.h"
#include "device_init.h"
#include "buzzer/buzzer32.h"

#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include "RFIDPN532/rfid_manager.h"
#include "Display19Inch/display19.h"
#include "RFIDPN532/rfidlua.h"

#include "UserButton/UserButton.h"
#include "UserButton/UserButtonlua.h"

#include "Lidar/lidar.h"
#include "Lidar/lidarlua.h"

#include "ForceSensor/ForceSensor.h"
#include "ForceSensor/ForceSensor_Lua.h"

#define USE_HSPI_PORT

#include <SPI.h>
#include <Wire.h>




extern RFIDReader rfid;
extern SPIClass spi_pn532_tft;
extern Adafruit_PN532 nfc;
extern TFT_eSPI tft;
extern UserButton userButton;
extern Lidar lidarBottom;
extern Lidar lidarTop;
extern ForceSensor forceSensorLeft;
extern ForceSensor forceSensorRight;


#endif // GLOBAL_H