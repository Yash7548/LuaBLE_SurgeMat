#include "global.h"


SPIClass spi_pn532_tft = NULL;
Adafruit_PN532 nfc = NULL;
TFT_eSPI tft = TFT_eSPI();
RFIDReader rfid;
UserButton userButton(USER_BUTTON);
Lidar lidarBottom;
Lidar lidarTop;

ForceSensor forceSensorLeft(FORCE_SENSOR_LEFT_PIN);
ForceSensor forceSensorRight(FORCE_SENSOR_RIGHT_PIN);

