#include "Global/global.h"

void initializeDevice()
{

    LittleFSFile::initFS();
    intializeStorage();
    initializeBLEController();
    buzzer_init_c();
    buzzer_set_speed_c(160); // Set default speed to 80ms
    buzzer_play_music_c("A2B2B2A2");
    initRfidDisplay();
    initializeUserButton();
    
   
   

}

void initializeBLEController(String deviceName)
{

    bleController.setBlePrefix(BLE_NAME_PREFIX);
    bleController.setBleProductUUID(BLE_PRODUCT_UUID);
    bleController.begin();
    bleController.setOnConnectCallback(handleBleConnect);
    bleController.setOnDisconnectCallback(handleBleDisconnect);
    bleController.setTextMessageCallback(lua_loop);
    bleController.setTextAbortCallback(luaClose);
    bleController.switchToTextMode();
    bleController.setOtaCallbacks(onOtaStart, onOtaProgress, onOtaSuccess, onOtaError);
    bleController.setTextQueueCallback(addStringToQueue);
    initializeBLEHandlers();
    initQueue();
    LLOGI("BLE Controller initialized");
}

void intializeStorage()
{

    STORAGE.initPreferences();
}


void initRfidDisplay()

{
    tft.init();
    tft.setRotation(3);
    spi_pn532_tft = tft.getSPIinstance();
    nfc = Adafruit_PN532(RFID_CS, &spi_pn532_tft);
    rfid.begin(nfc);

    // tft.setSwapBytes(true); // We need to swap the colour bytes (endianess)
    tft.fillScreen(TFT_BLACK);
    TJpgDec.setJpgScale(1);
    renderJPEG("/hyperlab.jpg");
    LLOGI("RFID Display initialized");
}

void initializeUserButton()
{
    userButton.begin(true);
    userButton.setLongPressTime(1000);
    userButton.setDoubleClickTime(250);
    userButton.setMultiClickTime(500);
    userButton.setMaxMultiClicks(7);
    setButtonInstance(&userButton);
    LLOGI("User Button initialized");
}

