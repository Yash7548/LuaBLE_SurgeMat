#include <Arduino.h>
#include "luatoswrapper.h"
#include "arduinobindings.h"

// Custom function to write to Serial from Lua
static int l_serial_write(lua_State* L) {
    const char* message = luaL_checkstring(L, 1);
    Serial.println(message);
    return 0;
}

// Custom LED control function
static int l_led_blink(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);
    int times = luaL_checkinteger(L, 2);
    int delay_ms = luaL_optinteger(L, 3, 500); // Optional delay, default 500ms
    
    pinMode(pin, OUTPUT);
    for(int i = 0; i < times; i++) {
        digitalWrite(pin, HIGH);
        delay(delay_ms);
        digitalWrite(pin, LOW);
        delay(delay_ms);
    }
    return 0;
}

// Lua output callback
static void lua_output_handler(const char* output) {
    Serial.print("Lua: ");
    Serial.println(output);
}

// Lua error callback
static void lua_error_handler(const char* error) {
    Serial.print("Error: ");
    Serial.println(error);
}

// Register custom functions
static void register_functions(lua_State* L) {
    // Register Arduino bindings first
    registerArduinoBindings(L);
    
    // Register our custom functions
    lua_register(L, "serial_write", l_serial_write);
    lua_register(L, "blink", l_led_blink);
    
    // Create system info table
    lua_newtable(L);
    lua_pushstring(L, "ESP32");
    lua_setfield(L, -2, "board");
    lua_pushinteger(L, ESP.getHeapSize());
    lua_setfield(L, -2, "heap_size");
    lua_setglobal(L, "SYSTEM");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nInitializing LuaWrapper example...");
    
    // Set callbacks
    lua_wrapper_set_output_cb(lua_output_handler);
    lua_wrapper_set_error_cb(lua_error_handler);
    lua_wrapper_set_register_cb(register_functions);
    
    // Initialize wrapper with 16KB stack
    if (!lua_wrapper_init(16384, 1)) {
        Serial.println("Failed to initialize Lua wrapper!");
        return;
    }
    
    Serial.println("Testing Lua execution...");

    // Example 1: Basic Arduino functions
    const char* test1 = R"(
        -- Configure built-in LED
        local LED_PIN = 2
        pinMode(LED_PIN, Arduino.OUTPUT)
        
        -- Blink LED using Arduino functions
        digitalWrite(LED_PIN, Arduino.HIGH)
        delay(1000)
        digitalWrite(LED_PIN, Arduino.LOW)
        
        serial_write("Basic Arduino test completed")
    )";
    
    if (!lua_wrapper_exec_string(test1, false, false)) {
        Serial.println("Test 1 failed!");
        return;
    }

    // Example 2: Custom functions
    const char* test2 = R"(
        -- Use custom blink function
        blink(2, 3, 200)  -- Pin 2, 3 times, 200ms delay
        
        -- Print system info
        serial_write("Board: " .. SYSTEM.board)
        serial_write("Heap size: " .. SYSTEM.heap_size)
    )";
    
    if (!lua_wrapper_exec_string(test2, false, false)) {
        Serial.println("Test 2 failed!");
        return;
    }

    // Example 3: Error handling
    const char* test3 = R"(
        -- Error handling example
        local status, err = pcall(function()
            nonexistent_function()  -- This will cause an error
        end)
        
        if not status then
            serial_write("Caught error: " .. err)
        end
    )";
    
    if (!lua_wrapper_exec_string(test3, false, false)) {
        Serial.println("Test 3 failed!");
        return;
    }

    // Print memory usage
    lua_wrapper_print_memory_usage();
    Serial.println("All tests completed!");
}

void loop() {
    // In real application, you might want to:
    // 1. Check for incoming serial commands
    // 2. Execute them using lua_wrapper_exec_string
    // 3. Handle any errors
    delay(1000);
}
