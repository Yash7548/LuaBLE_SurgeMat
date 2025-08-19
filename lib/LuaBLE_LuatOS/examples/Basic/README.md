# LuaWrapper Basic Example

This example demonstrates how to use the LuaWrapper with Arduino functions and custom C++ bindings.

## Key Features

1. Initialization
   ```cpp
   lua_wrapper_init(16384, 1);  // 16KB stack
   ```

2. Callback Registration
   ```cpp
   lua_wrapper_set_output_cb(output_handler);
   lua_wrapper_set_error_cb(error_handler);
   lua_wrapper_set_register_cb(register_functions);
   ```

3. Custom C++ Functions
   ```cpp
   // Register in C++
   lua_register(L, "serial_write", l_serial_write);
   lua_register(L, "blink", l_led_blink);
   
   // Use in Lua
   serial_write("Hello from Lua!")
   blink(2, 3, 200)  -- Pin 2, 3 times, 200ms
   ```

## BSP Configuration

The platformio.ini flags explained:

1. BOARD_HAS_PSRAM
   - Enables hardware PSRAM interface
   - Required for boards with external PSRAM

2. LUAT_USE_PSRAM
   - Allows LuatOS to utilize PSRAM
   - Provides more memory for scripts

3. LUAT_HEAP_SIZE=150*1024
   - Sets Lua heap to 150KB
   - Adjust based on your needs

## Example Output

When running the example, you should see:
```
Initializing LuaWrapper example...
Testing Lua execution...
Lua: Basic Arduino test completed
Lua: Board: ESP32
Lua: Heap size: XXXXX
Lua: Caught error: attempt to call a nil value
All tests completed!
```

## Included Tests

1. Basic Arduino Functions
   - GPIO operations
   - Delays
   - LED control

2. Custom Functions
   - Serial writing
   - LED blinking
   - System info access

3. Error Handling
   - Using pcall
   - Error callbacks
   - Safe execution

## Memory Management

The example includes memory monitoring:
- PSRAM usage tracking
- Heap monitoring
- Lua memory usage reporting

This example serves as a reference implementation showing how to:
1. Initialize the LuaWrapper
2. Register custom C++ functions
3. Handle errors safely
4. Manage memory effectively
