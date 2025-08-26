#include "Global/global.h"

void initializeDevice()
{

    LittleFSFile::initFS();
    intializeStorage();
    initializeBLEController();
    buzzer_init_c();
    buzzer_set_speed_c(160); // Set default speed to 80ms
    buzzer_play_music_c("A2B2B2A2");
    initializeLidar();
    initRfidDisplay();
    initializeUserButton();
    initializeForceSensor();
   
    
   
   

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

void initializeLidar()
{

    bool successtop = false;
    bool successbottom =false;

    Wire1.setPins(LIDAR_TOP_SDA, LIDAR_TOP_SCL);
    Wire.setPins(LIDAR_BOTTOM_SDA, LIDAR_BOTTOM_SCL);

    lidarTop.begin(&Wire1,LIDAR_TOP_SDA,LIDAR_TOP_SCL);

    if (lidarTop.start()) {
                LLOGI("Top LiDAR continuous mode started");
                successtop = true;
            } else {
                LLOGI("Failed to start Top LiDAR continuous mode");
                successtop = false;
            }


     delay(50);


    lidarBottom.begin(&Wire,LIDAR_BOTTOM_SDA,LIDAR_BOTTOM_SCL);

      if (lidarBottom.start()) {
                successbottom = true;
                LLOGI("Bottom LiDAR continuous mode started");
            } else {
                LLOGI("Failed to start Bottom LiDAR continuous mode");
                successbottom = false;
            }

             if(successbottom  && successtop){
        // buzzer.beepAsync(400, 200, 2);


    }
    else
    {
        // buzzer.beepAsync(200, 200, 3);

    }
}




void initializeForceSensor()
{
    // forceSensor.begin();
    // INFO_PRINTLN("Force Sensor initialized");
  // Initialize left force sensor
    forceSensorLeft.begin();
    LLOGI("Left Force Sensor initialized");
    
    // Initialize right force sensor
    forceSensorRight.begin();
    LLOGI("Right Force Sensor initialized");
    
    // Optional: Add a confirmation beep for successful initialization
    // buzzer.beepAsync(300, 100, 1);

}
