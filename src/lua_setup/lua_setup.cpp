#include "lua_setup.h"
#include "require_lua.h"

// Global state
bool bleConnected = false;

// Configuration
static struct
{
    bool autoLoadMainOnBoot = true;
    bool autoLoadMainOnBleDisconnect = true;
    const char *mainModule = "main";
} luaConfig;

// Forward declarations
static void registerCustomFunctions(lua_State *L);
static void outputHandler(const char *output);
static void errorHandler(const char *error);

void lua_setup()
{
    // Configure BLE callbacks

    // Set up Lua wrapper callbacks
    lua_wrapper_set_output_cb(outputHandler);
    lua_wrapper_set_error_cb(errorHandler);
    lua_wrapper_set_register_cb(registerCustomFunctions);
    lua_wrapper_set_main_module(luaConfig.mainModule);
    lua_wrapper_set_stop_module("stop");

    // Initialize Lua wrapper with 16KB stack
    if (!lua_wrapper_init(16384, 1))
    {
        Serial.println("Failed to initialize Lua wrapper!");
        return;
    }
    Serial.println("Lua wrapper initialized successfully");

    // Load main module on boot if BLE not connected
    if (!bleConnected && luaConfig.autoLoadMainOnBoot)
    {
        Serial.println("Loading main module...");
        lua_wrapper_exec_module(luaConfig.mainModule, false, false); // Not persistent, no auto-restart can be configured
    }

    Serial.println("System ready");
}

void lua_loop(String script)
{
    Serial.println("*****************LUA SCRIPT EXECUTE*****************");
    lua_wrapper_print_memory_usage();

    // Stop current execution if running
    if (lua_wrapper_is_running())
    {
        Serial.println("Stopping current execution...");
        lua_wrapper_stop(nullptr, STOP_CLEAN);

        // Wait for stop to complete
        int timeout = 10; // 5 seconds timeout
        while (lua_wrapper_is_running() && timeout > 0)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            lua_wrapper_stop(nullptr, STOP_CLEAN);
            timeout--;
        }
        if (lua_wrapper_is_running())
        {
            Serial.println("Warning: Script stop timeout - forcing cleanup");
            destroy_lua_state();
            create_lua_state();
        }

        // Check if stop completed successfully
        if (timeout <= 0)
        {
            Serial.println("Warning: Script stop timeout - forcing cleanup");
        }
        else
        {
            Serial.println("Script stopped successfully");
        }
    }

    // Execute the new script

    if (!lua_wrapper_exec_string(script.c_str(), false, false)) // no persistent, no auto-restart
    {
        Serial.println("Lua execution failed");
        Serial.print("Error: ");
        Serial.println(lua_wrapper_get_error());
    }
    else
    {
        // Serial.println("Lua Execution started successfully");
    }
    // lua_wrapper_print_memory_usage();
    // Serial.println("*****************LUA STOP*****************");
}

void luaClose(String stopScript)
{
    Serial.println("Lua abort requested");

    // remove first char if its a 0x01 which is used to indicate a stop request
    if (stopScript.length() > 0 && stopScript[0] == '\x01')
    {
        stopScript.remove(0, 1);
    }
    // print script size and content
    Serial.print("Stop script: ");
    Serial.println(stopScript.length());
    Serial.println(stopScript);

    if (stopScript.isEmpty())
    {
        // Just stop execution
        lua_wrapper_stop(nullptr, STOP_CLEAN);
    }
    else if (stopScript == "keep")
    {
        // Stop but keep state
        lua_wrapper_stop(nullptr, STOP_PERSISTENT);
    }
    else
    {
        // Stop with custom script
        lua_wrapper_stop(stopScript.c_str(), STOP_CLEAN);
    }
}

void handleBleConnect()
{
    bleConnected = true;
    Serial.println("BLE Connected");
    buzzer_play_music_c("A2B2");

    // Stop main module if running
    // if (lua_wrapper_is_running())
    // {
    //     Serial.println("Stopping main module for BLE session");
    //     lua_wrapper_stop(nullptr, STOP_CLEAN);
    // }

    Serial.println("Ready for BLE scripts");
}

void handleBleDisconnect()
{
    bleConnected = false;
    Serial.println("BLE Disconnected");
    buzzer_play_music_c("A2B2");

    // Stop any running BLE script
    // if (lua_wrapper_is_running())
    // {
    //     lua_wrapper_stop(nullptr, STOP_CLEAN);
    // }

    // Auto-load main if configured
    if (luaConfig.autoLoadMainOnBleDisconnect && lua_wrapper_is_running() == false)
    {
        // If auto-load is enabled, reload the main module if nothing is running
        Serial.println("Auto-loading main module on BLE disconnect...");
        // This allows the main module to handle BLE disconnection gracefully
        Serial.println("Reloading main module...");
        lua_wrapper_exec_module(luaConfig.mainModule, true, false);
    }
}

// Callbacks
static void outputHandler(const char *output)
{
    Serial.println(output);

    // Send to BLE if connected
    if (bleConnected && bleController.deviceConnected)
    {
        bleController.sendTextOutput(output);
    }
}

static void errorHandler(const char *error)
{
    Serial.print("Lua Error: ");
    Serial.println(error);

    // Send to BLE if connected
    if (bleConnected && bleController.deviceConnected)
    {
        // String errorMsg = "Error: ";
        // errorMsg += error;
        // bleController.sendTextOutput(errorMsg.c_str());
        // form a json error message
        JsonDocument doc;
        doc["msgtyp"] = "error";
        doc["message"] = error;
        doc["timestamp"] = millis();
        bleController.sendMessage(doc);
    }
}

// Custom Lua functions
static int lua_ble_print(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    if (bleController.deviceConnected)
    {
        bleController.sendTextOutput(str);
    }
    return 0;
}

static int lua_exit(lua_State *L)
{
    bleController.switchToJsonMode();
    return 0;
}

static void registerCustomFunctions(lua_State *L)
{
    // Register custom SPIFFS require function first
    register_custom_require(L);

    luaopen_buzzer32(L);

       // Register display functions
    lua_register_display19(L);

    // Register RFID functions
    lua_register_rfid(L);
    lua_register_userbutton(L);

    // Register LiDAR functions
    lua_register_lidar(L);

    // Register Force Sensor functions
    lua_register_forcesensor(L);

    // Register BLE functions
    lua_register(L, "ble_print", lua_ble_print);
    lua_register(L, "exit", lua_exit);

    // Create Device table
    lua_newtable(L);

    lua_pushstring(L, "1.0.0");
    lua_setfield(L, -2, "version");

    lua_pushstring(L, "MyDevice");
    lua_setfield(L, -2, "name");

    lua_pushinteger(L, ESP.getChipRevision());
    lua_setfield(L, -2, "chipRev");

    lua_pushinteger(L, ESP.getFreeHeap());
    lua_setfield(L, -2, "freeHeap");

    lua_setglobal(L, "Device");

    // Register constants
    lua_pushinteger(L, 12345);
    lua_setglobal(L, "DEVICE_ID");

    lua_pushnumber(L, 3.14159);
    lua_setglobal(L, "PI");

    lua_pushstring(L, "1.0.0");
    lua_setglobal(L, "VERSION");

    lua_pushboolean(L, true);
    lua_setglobal(L, "DEBUG_MODE");
}