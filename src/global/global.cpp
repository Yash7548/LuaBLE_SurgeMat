#include "global.h"


SPIClass spi_pn532_tft = NULL;
Adafruit_PN532 nfc = NULL;
TFT_eSPI tft = TFT_eSPI();
RFIDReader rfid;
UserButton userButton(USER_BUTTON);

