#include "require_lua.h"
#include "Arduino.h"
#include "LittleFS.h"

#define LITTLEFS LittleFS


// Read file line by line and execute in Lua
static int load_file_line_by_line(lua_State *L, const char *filepath) {
    File file = LITTLEFS.open(filepath, "r");
    if (!file) {
        Serial.printf("Failed to open file: %s\n", filepath);
        return LUA_ERRFILE;
    }
    
    Serial.printf("Reading file line by line: %s\n", filepath);
    
    String luaCode = "";
    String line;
    int lineNumber = 0;
    
    // Read file line by line
    while (file.available()) {
        line = file.readStringUntil('\n');
        lineNumber++;
        
        // Add line to complete code
        luaCode += line + "\n";
        
        Serial.printf("Line %d: %s\n", lineNumber, line.c_str());
    }
    
    file.close();
    Serial.printf("Read %d lines from %s\n", lineNumber, filepath);
    
    // Load the complete code into Lua
    int result = luaL_loadbuffer(L, luaCode.c_str(), luaCode.length(), filepath);
    if (result != LUA_OK) {
        Serial.printf("Lua load error: %s\n", lua_tostring(L, -1));
        return result;
    }
    
    Serial.printf("Successfully loaded code from %s\n", filepath);
    return LUA_OK;
}

// Check if file exists in LITTLEFS
bool spiffs_file_exists(const char* filepath) {
    if (!filepath) return false;
    return LITTLEFS.exists(filepath);
}

int lua_custom_require(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    lua_settop(L, 1);  // LOADED table will be at index 2
    
    // Get the LOADED table
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    lua_getfield(L, 2, name);  // LOADED[name]
    
    if (lua_toboolean(L, -1)) {
        // Package is already loaded
        Serial.printf("Module '%s' already loaded\n", name);
        return 1;
    }
    
    // Module not loaded yet, remove the nil value
    lua_pop(L, 1);
    
    Serial.printf("Loading module directly: %s\n", name);
    
    // Construct file path (assume .lua extension and root path)
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/%s.lua", name);
    
    // Check if file exists
    if (!spiffs_file_exists(filepath)) {
        Serial.printf("File not found: %s\n", filepath);
        return luaL_error(L, "module '%s' not found at path '%s'", name, filepath);
    }
    
    // Load file line by line
    int loadResult = load_file_line_by_line(L, filepath);
    if (loadResult != LUA_OK) {
        return luaL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          name, filepath, lua_tostring(L, -1));
    }
    
    // Execute the loaded code
    Serial.printf("Executing module: %s\n", name);
    lua_pushstring(L, filepath);  // Pass filepath as argument
    
    int execResult = lua_pcall(L, 1, 1, 0);  // Call with 1 arg, expect 1 result
    if (execResult != LUA_OK) {
        return luaL_error(L, "error executing module '%s':\n\t%s",
                          name, lua_tostring(L, -1));
    }
    
    // Store the result in LOADED table
    if (!lua_isnil(L, -1)) {
        lua_setfield(L, 2, name);  // LOADED[name] = returned value
    }
    
    // Get the final result
    if (lua_getfield(L, 2, name) == LUA_TNIL) {
        // Module set no value, use true as result
        lua_pushboolean(L, 1);
        lua_pushvalue(L, -1);  // Extra copy to be returned
        lua_setfield(L, 2, name);  // LOADED[name] = true
    }
    
    Serial.printf("Module '%s' loaded and executed successfully\n", name);
    return 1;
}

void register_custom_require(lua_State *L) {
    // Register our custom require function
    lua_pushcfunction(L, lua_custom_require);
    lua_setglobal(L, "requiree");
    
    Serial.println("Custom LITTLEFS require function registered");
    
    // Ensure LOADED table exists
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    lua_pop(L, 1);  // Remove the table from stack
    
    Serial.println("LOADED table initialized");
} 