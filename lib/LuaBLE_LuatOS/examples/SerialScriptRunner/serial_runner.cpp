#include <Arduino.h>
#include "luatoswrapper.h"
#include "arduinobindings.h"

// Buffer for incoming serial data
String inputBuffer = "";
bool scriptRunning = false;

// Math operation functions
static int l_add_numbers(lua_State* L) {
    // Get number of arguments
    int n = lua_gettop(L);
    double sum = 0;
    
    // Sum all numbers passed
    for(int i = 1; i <= n; i++) {
        sum += luaL_checknumber(L, i);
    }
    
    // Return sum
    lua_pushnumber(L, sum);
    return 1;
}

static int l_calc_stats(lua_State* L) {
    double num1 = luaL_checknumber(L, 1);
    double num2 = luaL_checknumber(L, 2);
    
    // Calculate multiple results
    double sum = num1 + num2;
    double diff = num1 - num2;
    double prod = num1 * num2;
    double quot = num2 != 0 ? num1 / num2 : 0;
    
    // Push multiple return values
    lua_pushnumber(L, sum);   // Sum
    lua_pushnumber(L, diff);  // Difference
    lua_pushnumber(L, prod);  // Product
    lua_pushnumber(L, quot);  // Quotient
    
    return 4;  // Number of return values
}

// Table manipulation example
static int l_create_point(lua_State* L) {
    double x = luaL_checknumber(L, 1);
    double y = luaL_checknumber(L, 2);
    
    // Create a table for the point
    lua_newtable(L);
    
    // Set x coordinate
    lua_pushstring(L, "x");
    lua_pushnumber(L, x);
    lua_settable(L, -3);
    
    // Set y coordinate
    lua_pushstring(L, "y");
    lua_pushnumber(L, y);
    lua_settable(L, -3);
    
    // Add a method to calculate distance from origin
    lua_pushstring(L, "distance");
    lua_pushcfunction(L, [](lua_State* L) -> int {
        // Get 'self' table
        luaL_checktype(L, 1, LUA_TTABLE);
        
        // Get x and y values
        lua_getfield(L, 1, "x");
        lua_getfield(L, 1, "y");
        double x = lua_tonumber(L, -2);
        double y = lua_tonumber(L, -1);
        
        // Calculate distance
        double dist = sqrt(x*x + y*y);
        lua_pushnumber(L, dist);
        return 1;
    });
    lua_settable(L, -3);
    
    return 1;  // Return the table
}

// Custom function to write to Serial from Lua
static int l_serial_write(lua_State* L) {
    const char* message = luaL_checkstring(L, 1);
    Serial.println(message);
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
    scriptRunning = false;
}

// Register custom functions
static void register_functions(lua_State* L) {
    // Register Arduino bindings first
    registerArduinoBindings(L);
    
    // Register our custom functions
    lua_register(L, "serial_write", l_serial_write);
    lua_register(L, "add", l_add_numbers);
    lua_register(L, "calc_stats", l_calc_stats);
    lua_register(L, "create_point", l_create_point);
    
    // Create math constants table
    lua_newtable(L);
    lua_pushnumber(L, M_PI);
    lua_setfield(L, -2, "pi");
    lua_pushnumber(L, M_E);
    lua_setfield(L, -2, "e");
    lua_setglobal(L, "MATH_CONST");
    
    // Create system info table
    lua_newtable(L);
    lua_pushstring(L, "ESP32");
    lua_setfield(L, -2, "board");
    lua_pushinteger(L, ESP.getHeapSize());
    lua_setfield(L, -2, "heap_size");
    lua_setglobal(L, "SYSTEM");
}

void printHelp() {
    Serial.println("\nLua Script Runner Commands:");
    Serial.println("1. Run script: Send your Lua code followed by '###'");
    Serial.println("2. Stop script: Send 'STOP'");
    Serial.println("3. Show help: Send 'HELP'");
    Serial.println("4. Memory info: Send 'MEM'");
    Serial.println("\nExample Scripts:");
    
    Serial.println("\n1. Basic Loop:");
    Serial.println("while true do\n  print('Hello')\n  delay(1000)\nend\n###\n");
    
    Serial.println("2. Add Numbers:");
    Serial.println("result = add(10, 20, 30)\nprint('Sum:', result)###\n");
    
    Serial.println("3. Multiple Return Values:");
    Serial.println("sum, diff, prod, quot = calc_stats(10, 5)\n"
                  "print('Sum:', sum)\n"
                  "print('Difference:', diff)\n"
                  "print('Product:', prod)\n"
                  "print('Quotient:', quot)###\n");
    
    Serial.println("4. Object Creation:");
    Serial.println("point = create_point(3, 4)\n"
                  "print('X:', point.x)\n"
                  "print('Y:', point.y)\n"
                  "print('Distance:', point:distance())###\n");
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("\nInitializing Lua Script Runner...");
    
    // Set callbacks
    lua_wrapper_set_output_cb(lua_output_handler);
    lua_wrapper_set_error_cb(lua_error_handler);
    lua_wrapper_set_register_cb(register_functions);
    
    // Initialize wrapper with 16KB stack
    if (!lua_wrapper_init(16384, 1)) {
        Serial.println("Failed to initialize Lua wrapper!");
        return;
    }
    
    printHelp();
    Serial.println("Ready for scripts!");
}

void processSerialCommand() {
    while (Serial.available()) {
        char c = Serial.read();
        
        // Check for command termination
        if (inputBuffer.endsWith("###")) {
            // Remove the terminator
            inputBuffer = inputBuffer.substring(0, inputBuffer.length() - 3);
            
            // Execute the script
            Serial.println("\nExecuting script:");
            Serial.println(inputBuffer);
            Serial.println("-------------------");
            
            scriptRunning = true;
            if (!lua_wrapper_exec_string(inputBuffer.c_str(), false, false)) {
                Serial.println("Script execution failed!");
                scriptRunning = false;
            }
            
            inputBuffer = ""; // Clear buffer
            return;
        }
        
        // Check for special commands
        if (c == '\n' || c == '\r') {
            if (inputBuffer.equals("STOP")) {
                Serial.println("Stopping script...");
                lua_wrapper_stop(nullptr, STOP_CLEAN);
                scriptRunning = false;
                inputBuffer = "";
                return;
            }
            else if (inputBuffer.equals("HELP")) {
                printHelp();
                inputBuffer = "";
                return;
            }
            else if (inputBuffer.equals("MEM")) {
                lua_wrapper_print_memory_usage();
                inputBuffer = "";
                return;
            }
            // Ignore empty lines
            if (inputBuffer.length() == 0) return;
        }
        
        // Add character to buffer
        inputBuffer += c;
    }
}

void loop() {
    // Process any incoming serial data
    processSerialCommand();
    
    // Small delay to prevent watchdog reset
    delay(1);
}
