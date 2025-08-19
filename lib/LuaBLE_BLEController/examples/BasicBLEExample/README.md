# Basic BLE Example with Text Mode

This example demonstrates how to use the BLEController library in text mode with Lua script handling. It shows how to:
- Initialize the BLE controller
- Set up BLE connection callbacks
- Handle text-based messages
- Process Lua scripts received over BLE

## Hardware Required
- ESP32 board

## Dependencies
- NimBLE-Arduino

## Configuration
Before running the example:
1. Update `BLE_NAME_PREFIX` if you want to change the device name prefix (default is "BLE")
2. Set your `BLE_PRODUCT_UUID` to a unique identifier for your product

## Features Demonstrated
- BLE device initialization and configuration
- Connection/Disconnection event handling
- Text mode message processing
- Bidirectional communication between BLE and Serial
- Lua script handling

## How it Works
1. The device initializes with the specified BLE name prefix
2. It sets up callbacks for various BLE events:
   - Connection events (connect/disconnect)
   - Text message handling callback
3. Switches to text mode for Lua script handling
4. Continuously processes:
   - BLE text messages
   - Serial input (echoed back through BLE)

## Serial Monitor Output
Connect to Serial Monitor at 115200 baud to see:
- BLE connection status
- Received Lua scripts
- Echo of serial input
- Error messages (if any)

## Example Output
```
BLE Controller initialized in Text Mode
BLE Connected!
Received text: print("Hello from Lua!")
Executing Lua script:
print("Hello from Lua!")
```

## Usage Notes
- Send text messages through BLE to simulate Lua script execution
- Type in the Serial monitor to have messages sent back through BLE
- The example demonstrates basic text handling infrastructure
- Actual Lua script execution should be implemented based on your needs
