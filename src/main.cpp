#include <Arduino.h>
#include "Global/global.h"




void setup()
{
    // Configure the wrapper
    Serial.begin(115200);
 
    
    initializeDevice();
    lua_setup();
    
    Serial.println("Setup complete new firmware 22222");
}

void loop()
{
    // Handle serial commands
    processBLEMessages();
    bleController.processOTAData();
    
    // Charger status updates are now handled by the RTOS task
    // No need for manual updates in the main loop


}
