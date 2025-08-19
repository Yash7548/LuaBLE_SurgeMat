#include <Arduino.h>
#include <BLEController.h>

// BLE Configuration
#define BLE_NAME_PREFIX "HLAB!"  // Your device name prefix
#define BLE_PRODUCT_UUID "0xAE05" // Your product UUID

BLEController bleController;
String receivedLuaScript = "";

// BLE callback functions
void onConnect() {
    Serial.println("BLE Connected!");
}

void onDisconnect() {
    Serial.println("BLE Disconnected!");
    receivedLuaScript = ""; // Clear script on disconnect
}

// Text message callback for Lua script execution
void onTextMessage(String message) {
    Serial.println("Received text: " + message);
    
    // In this example, we just echo the received text back through Serial
    // You can implement Lua script execution here
    Serial.println("Executing Lua script:");
    Serial.println(message);
    
    // Echo back confirmation

}

void onTextAbort(String msg) {
    Serial.println("Lua script aborted: " + msg);
    receivedLuaScript = ""; // Clear script on abort
}


void processBLEMessages() {
    bleController.processTextMessages();
}

void setup() {
    // Initialize Serial for debugging
    Serial.begin(115200);
    
    // Initialize BLE Controller
    bleController.setBlePrefix(BLE_NAME_PREFIX);
    bleController.setBleProductUUID(BLE_PRODUCT_UUID);
    bleController.begin();  // Uses default name generation
    
    // Set callbacks
    bleController.setOnConnectCallback(onConnect);
    bleController.setOnDisconnectCallback(onDisconnect);
    bleController.setTextMessageCallback(onTextMessage);
    bleController.setTextAbortCallback(onTextAbort);
    bleController.registerMessageCallback("sensordata", [](JsonDocument &doc) {
        // Handle sensor data messages
        Serial.println("Sensor data received: " + doc.as<String>());
    });

    // Switch to text mode for Lua script handling
    bleController.switchToTextMode();
    // Optionally, 
    // bleController.switchToJsonMode(); // If you want to handle JSON messages with registerMessageCallback
    
    Serial.println("BLE Controller initialized in Text Mode");
}

void loop() {
    // Handle BLE messages
    processBLEMessages();
    
    // Handle any serial input
    if (Serial.available()) {
        String serialInput = Serial.readStringUntil('\n');
        // Echo serial input back through BLE
        bleController.sendMessage(serialInput.c_str());
    }
}
