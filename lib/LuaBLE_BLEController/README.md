# BLEController Library

A comprehensive BLE (Bluetooth Low Energy) controller library for ESP32, featuring text mode for Lua script handling, JSON message processing, and OTA update capabilities.

## Features
- Text mode for Lua script handling
- JSON message processing
- BLE device name auto-generation
- OTA (Over-The-Air) update support
- SPIFFS file transfer capabilities
- Connection status management
- Battery level and version reporting

## Installation

1. Install required dependencies:
   - NimBLE-Arduino
   - ArduinoJson

2. Copy the BLEController library to your project's lib directory

## API Reference

### Initialization
```cpp
BLEController bleController;
bleController.setBlePrefix("BLE");              // Set device name prefix
bleController.setBleProductUUID("your-uuid");   // Set product UUID
bleController.begin();                          // Initialize with auto-generated name
// or
bleController.begin("CustomName");              // Initialize with custom name
```

### Message Handling Modes

The BLEController library supports two exclusive operating modes - Text Mode and JSON Mode. Only one mode can be active at a time.

#### Text Mode
```cpp
// Switch to text mode (disables JSON mode)
bleController.switchToTextMode();

// Set callbacks for text mode
bleController.setTextMessageCallback([](String message) {
    // Handle incoming text/Lua script
});

bleController.setTextAbortCallback([](String message) {
    // Handle abort signal
});

bleController.setTextQueueCallback([](String message) {
    // Handle queued messages
});
```

#### JSON Mode
```cpp
// Switch to JSON mode (disables text mode)
bleController.switchToJsonMode();

// Register callback for specific message type
bleController.registerMessageCallback("status", [](JsonDocument& doc) {
    // Handle JSON message with msgtype="status"
});

// Register high priority callback for specific message type
bleController.registerHighPriorityCallback("error", [](JsonDocument& doc) {
    // Handle high priority JSON message with msgtype="error"
});

// Send JSON message
JsonDocument doc;
doc["msgtype"] = "status";
doc["value"] = "ok";
bleController.sendMessage(doc);
```

**Important**: 
- You must explicitly switch modes using `switchToTextMode()` or `switchToJsonMode()`
- Switching modes clears any existing message buffers
- In JSON mode, callbacks are triggered based on the `msgtype` field in the JSON message
- High priority callbacks are processed before regular callbacks

### Connection Callbacks
```cpp
bleController.setOnConnectCallback(onConnect);       // Connection handler
bleController.setOnDisconnectCallback(onDisconnect); // Disconnection handler
```

### OTA Update Handling
```cpp
bleController.setOtaCallbacks(
    onStart,     // Called when OTA starts
    onProgress,  // Called with progress updates
    onSuccess,   // Called on successful completion
    onError      // Called if error occurs
);
```

### Message Processing
```cpp
bleController.processJsonMessages();  // Process JSON messages
bleController.processTextMessages();  // Process text messages
bleController.processOTAData();      // Process OTA updates
```

### File Transfer
```cpp
bleController.transferFileFromSPIFFS(filename); // Transfer file via BLE
```

### Battery and Version Management
```cpp
// Update battery level only
bleController.updateBattery(75);  // Set battery level to 75%

// Update battery level, software version and hardware version
bleController.updateBatteryAndVersion(
    75,                     // Battery level (0-100)
    softwareVersion,        // Software version array
    softwareVersionLength,
    hardwareVersion,        // Hardware version array
    hardwareVersionLength
);
```

## BLE Device Name Generation

The library uses a smart device naming system:
1. Base name can be set via `setBlePrefix()`
2. If no custom name provided to `begin()`, auto-generates using:
   - Prefix (default: "BLE")
   - Product UUID
   - Unique device identifier

Format: `{prefix}_{device_id}`

## Callback System

### Text Mode Callbacks
- `TextMessageCallback`: Handles incoming text messages/Lua scripts
- `TextAbortCallback`: Handles abort signals
- `TextQueueCallback`: Manages message queue

### JSON Mode Callbacks
- `MessageCallback`: Processes JSON messages
- `HighPriorityCallback`: Handles priority messages

### System Callbacks
- `OnConnectCallback`: Connection established
- `OnDisconnectCallback`: Connection lost
- `OtaCallbacks`: OTA update events

## Usage Examples

### Basic Text Mode Setup
```cpp
void onTextMessage(String message) {
    Serial.println("Received: " + message);
    // Process text/Lua script
}

void setup() {
    bleController.setBlePrefix("LUA");
    bleController.begin();
    bleController.setTextMessageCallback(onTextMessage);
    bleController.switchToTextMode();
}

void loop() {
    bleController.processTextMessages();
}
```

### JSON Mode with Message Type Handling
```cpp
// Callback for specific message type
void handleStatusMessage(JsonDocument& doc) {
    // Handle status message
    String status = doc["status"].as<String>();
    int value = doc["value"];
    Serial.printf("Status: %s, Value: %d\n", status.c_str(), value);
}

// Callback for OTA updates
void onOtaStart() {
    Serial.println("OTA update started");
    JsonDocument response;
    response["msgtype"] = "otastart";
    bleController.sendMessage(response);
}

void setup() {
    bleController.begin();
    bleController.switchToJsonMode();  // Must switch to JSON mode first
    
    // Register callbacks for specific message types
    bleController.registerMessageCallback("status", handleStatusMessage);
    bleController.registerMessageCallback("config", [](JsonDocument& doc) {
        // Handle config message
        String config = doc["config"].as<String>();
        Serial.println("Received config: " + config);
    });
    
    // Register high priority error handler
    bleController.registerHighPriorityCallback("error", [](JsonDocument& doc) {
        String error = doc["error"].as<String>();
        Serial.println("Error received: " + error);
    });
    
    // Setup OTA handling
    bleController.setOtaCallbacks(
        onOtaStart,
        [](int progress) { Serial.printf("OTA: %d%%\n", progress); },
        []() { Serial.println("OTA success"); },
        [](const char* error) { Serial.printf("OTA error: %s\n", error); }
    );
}

void loop() {
    // Process any received messages based on their msgtype
    bleController.processJsonMessages();
    
    // Optional: Process any OTA data if needed
    bleController.processOTAData();
    
    // Example of sending a message
    if (someCondition) {
        JsonDocument doc;
        doc["msgtype"] = "status";
        doc["status"] = "running";
        doc["value"] = 42;
        bleController.sendMessage(doc);
    }
}
```

## Best Practices

1. Message Processing
   - Call process functions regularly in loop()
   - Handle one mode at a time (text or JSON)
   - Check connection status before sending messages

2. OTA Updates
   - Implement all OTA callbacks
   - Monitor progress and handle errors
   - Restart device after successful update

3. Memory Management
   - Clear message buffers when disconnected
   - Use appropriate buffer sizes for messages
   - Handle fragmented messages properly

## Error Handling

The library provides several error handling mechanisms:
- OTA error callbacks
- Connection state monitoring
- Message processing status
- File transfer status updates

Handle these appropriately in your application for robust operation.

## Contributing

Contributions welcome! Please read our contributing guidelines and send pull requests.

## License

This project is licensed under the MIT License - see the LICENSE file for details.
