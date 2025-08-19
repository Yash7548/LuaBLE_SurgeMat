



#include "arduinoBindings.h"


#undef  UART_SCLK_APB
#undef  UART_SCLK_RTC
#undef  UART_SCLK_XTAL
#undef  UART_SCLK_DEFAULT
#include <Arduino.h>
// Time functions
int l_millis(lua_State* L) {
    lua_pushinteger(L, millis());
    return 1;
}

int l_micros(lua_State* L) {
    lua_pushinteger(L, micros());
    return 1;
}

int l_delay(lua_State* L) {
    int ms = luaL_checkinteger(L, 1);
    if (ms > 0) {
        delay(ms);
    }
    return 0;
}

int l_delayMicroseconds(lua_State* L) {
    int us = luaL_checkinteger(L, 1);
    if (us > 0) {
        delayMicroseconds(us);
    }
    return 0;
}

// Digital I/O functions
int l_pinMode(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);
    int mode = luaL_checkinteger(L, 2);
    pinMode(pin, mode);
    return 0;
}

int l_digitalWrite(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);
    int value = luaL_checkinteger(L, 2);
    digitalWrite(pin, value);
    return 0;
}

int l_digitalRead(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);
    lua_pushinteger(L, digitalRead(pin));
    return 1;
}

// Analog I/O functions
int l_analogRead(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);
    lua_pushinteger(L, analogRead(pin));
    return 1;
}

int l_analogWrite(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);
    int value = luaL_checkinteger(L, 2);
    #ifdef ESP32
        // ESP32 doesn't have built-in analogWrite, would need LEDC
        // This is a placeholder - implement PWM if needed
        lua_pushstring(L, "analogWrite not implemented for ESP32");
        lua_error(L);
    #else
        analogWrite(pin, value);
    #endif
    return 0;
}

int l_analogReadResolution(lua_State* L) {
    int bits = luaL_checkinteger(L, 1);
    #ifdef ESP32
        analogReadResolution(bits);
    #endif
    return 0;
}

int l_analogWriteResolution(lua_State* L) {
    int bits = luaL_checkinteger(L, 1);
    #ifdef ESP32
        // analogWriteResolution(bits);
    #endif
    return 0;
}

// Pulse functions
int l_pulseIn(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);
    int value = luaL_checkinteger(L, 2);
    unsigned long timeout = luaL_optinteger(L, 3, 1000000);
    lua_pushinteger(L, pulseIn(pin, value, timeout));
    return 1;
}

int l_pulseInLong(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);
    int value = luaL_checkinteger(L, 2);
    unsigned long timeout = luaL_optinteger(L, 3, 1000000);
    lua_pushinteger(L, pulseInLong(pin, value, timeout));
    return 1;
}

// Tone functions
int l_tone(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);
    int frequency = luaL_checkinteger(L, 2);
    int duration = luaL_optinteger(L, 3, 0);
    
    #ifdef ESP32
        // ESP32 doesn't have built-in tone
        lua_pushstring(L, "tone not implemented for ESP32");
        lua_error(L);
    #else
        if (duration > 0) {
            tone(pin, frequency, duration);
        } else {
            tone(pin, frequency);
        }
    #endif
    return 0;
}

int l_noTone(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);
    #ifdef ESP32
        // ESP32 doesn't have built-in noTone
        lua_pushstring(L, "noTone not implemented for ESP32");
        lua_error(L);
    #else
        noTone(pin);
    #endif
    return 0;
}

// Random functions
int l_random(lua_State* L) {
    int n = lua_gettop(L);
    if (n == 0) {
        // No arguments - return 0-RAND_MAX
        lua_pushinteger(L, random());
    } else if (n == 1) {
        // One argument - return 0 to (max-1)
        long max = luaL_checkinteger(L, 1);
        lua_pushinteger(L, random(max));
    } else {
        // Two arguments - return min to (max-1)
        long min = luaL_checkinteger(L, 1);
        long max = luaL_checkinteger(L, 2);
        lua_pushinteger(L, random(min, max));
    }
    return 1;
}

int l_randomSeed(lua_State* L) {
    unsigned long seed = luaL_checkinteger(L, 1);
    randomSeed(seed);
    return 0;
}

// Math utilities
int l_map(lua_State* L) {
    long value = luaL_checkinteger(L, 1);
    long fromLow = luaL_checkinteger(L, 2);
    long fromHigh = luaL_checkinteger(L, 3);
    long toLow = luaL_checkinteger(L, 4);
    long toHigh = luaL_checkinteger(L, 5);
    lua_pushinteger(L, map(value, fromLow, fromHigh, toLow, toHigh));
    return 1;
}

int l_constrain(lua_State* L) {
    long value = luaL_checkinteger(L, 1);
    long min = luaL_checkinteger(L, 2);
    long max = luaL_checkinteger(L, 3);
    lua_pushinteger(L, constrain(value, min, max));
    return 1;
}

// Print functions
int l_print(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++) {
        if (i > 1) Serial.print("\t");
        
        if (lua_isstring(L, i)) {
            Serial.print(lua_tostring(L, i));
        } else if (lua_isnumber(L, i)) {
            Serial.print(lua_tonumber(L, i));
        } else if (lua_isboolean(L, i)) {
            Serial.print(lua_toboolean(L, i) ? "true" : "false");
        } else if (lua_isnil(L, i)) {
            Serial.print("nil");
        } else {
            Serial.print(luaL_typename(L, i));
        }
    }
    return 0;
}

int l_println(lua_State* L) {
    l_print(L);  // Print all arguments
    Serial.println();  // Add newline
    return 0;
}

// Helper function: typeof
int l_typeof(lua_State* L) {
    if (lua_gettop(L) < 1) {
        lua_pushstring(L, "nil");
    } else {
        lua_pushstring(L, luaL_typename(L, 1));
    }
    return 1;
}

// Helper function: tostring (enhanced)
int l_tostring(lua_State* L) {
    if (lua_gettop(L) < 1) {
        lua_pushstring(L, "");
        return 1;
    }
    
    switch (lua_type(L, 1)) {
        case LUA_TSTRING:
            lua_pushvalue(L, 1);
            break;
        case LUA_TNUMBER:
            lua_pushstring(L, String(lua_tonumber(L, 1)).c_str());
            break;
        case LUA_TBOOLEAN:
            lua_pushstring(L, lua_toboolean(L, 1) ? "true" : "false");
            break;
        case LUA_TNIL:
            lua_pushstring(L, "nil");
            break;
        default:
            lua_pushstring(L, luaL_typename(L, 1));
            break;
    }
    return 1;
}

// Helper function: tonumber (safe)
int l_tonumber(lua_State* L) {
    if (lua_gettop(L) < 1) {
        lua_pushnil(L);
        return 1;
    }
    
    if (lua_isnumber(L, 1)) {
        lua_pushvalue(L, 1);
    } else if (lua_isstring(L, 1)) {
        const char* str = lua_tostring(L, 1);
        char* endptr;
        double num = strtod(str, &endptr);
        if (*endptr == '\0') {
            lua_pushnumber(L, num);
        } else {
            lua_pushnil(L);
        }
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// Helper function: len (get length of string/table)
int l_len(lua_State* L) {
    if (lua_gettop(L) < 1) {
        lua_pushinteger(L, 0);
        return 1;
    }
    
    if (lua_isstring(L, 1)) {
        size_t len;
        lua_tolstring(L, 1, &len);
        lua_pushinteger(L, len);
    } else if (lua_istable(L, 1)) {
        lua_pushinteger(L, lua_rawlen(L, 1));
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

// Helper function: keys (get table keys)
int l_keys(lua_State* L) {
    if (!lua_istable(L, 1)) {
        lua_newtable(L);
        return 1;
    }
    
    lua_newtable(L);  // Result table
    int index = 1;
    
    lua_pushnil(L);  // First key
    while (lua_next(L, 1) != 0) {
        // Copy key
        lua_pushvalue(L, -2);
        // Set in result table
        lua_rawseti(L, -4, index++);
        // Remove value, keep key for next iteration
        lua_pop(L, 1);
    }
    
    return 1;
}

// Helper function: values (get table values)
int l_values(lua_State* L) {
    if (!lua_istable(L, 1)) {
        lua_newtable(L);
        return 1;
    }
    
    lua_newtable(L);  // Result table
    int index = 1;
    
    lua_pushnil(L);  // First key
    while (lua_next(L, 1) != 0) {
        // Set value in result table
        lua_rawseti(L, -3, index++);
        // Key is already on stack for next iteration
    }
    
    return 1;
}

// Helper function: dump (debug print any value)
int l_dump(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++) {
        if (i > 1) Serial.print(", ");
        
        switch (lua_type(L, i)) {
            case LUA_TSTRING:
                Serial.print("\"");
                Serial.print(lua_tostring(L, i));
                Serial.print("\"");
                break;
            case LUA_TNUMBER:
                Serial.print(lua_tonumber(L, i));
                break;
            case LUA_TBOOLEAN:
                Serial.print(lua_toboolean(L, i) ? "true" : "false");
                break;
            case LUA_TNIL:
                Serial.print("nil");
                break;
            case LUA_TTABLE:
                Serial.print("table{");
                // Simple table dump
                lua_pushnil(L);
                bool first = true;
                while (lua_next(L, i) != 0) {
                    if (!first) Serial.print(", ");
                    first = false;
                    
                    // Print key
                    if (lua_type(L, -2) == LUA_TSTRING) {
                        Serial.print(lua_tostring(L, -2));
                    } else {
                        Serial.print("[");
                        Serial.print(lua_tonumber(L, -2));
                        Serial.print("]");
                    }
                    Serial.print("=");
                    
                    // Print value type
                    Serial.print(luaL_typename(L, -1));
                    
                    lua_pop(L, 1);
                }
                Serial.print("}");
                break;
            // default:
            //     Serial.print(luaL_typename(L, i));
            //     break;
        }
    }
    Serial.println();
    return 0;
}

// Register all bindings
void registerArduinoBindings(lua_State* L) {
    // Time functions
    lua_register(L, "millis", l_millis);
    lua_register(L, "micros", l_micros);
    lua_register(L, "delay", l_delay);
    lua_register(L, "delayMicroseconds", l_delayMicroseconds);
    
    // Digital I/O
    lua_register(L, "pinMode", l_pinMode);
    lua_register(L, "digitalWrite", l_digitalWrite);
    lua_register(L, "digitalRead", l_digitalRead);
    
    // Analog I/O
    lua_register(L, "analogRead", l_analogRead);
    lua_register(L, "analogWrite", l_analogWrite);
    lua_register(L, "analogReadResolution", l_analogReadResolution);
    lua_register(L, "analogWriteResolution", l_analogWriteResolution);
    
    // Pulse functions
    lua_register(L, "pulseIn", l_pulseIn);
    lua_register(L, "pulseInLong", l_pulseInLong);
    
    // Tone functions (with ESP32 check)
    #ifndef ESP32
    lua_register(L, "tone", l_tone);
    lua_register(L, "noTone", l_noTone);
    #endif
    
    // Random functions
    lua_register(L, "random", l_random);
    lua_register(L, "randomSeed", l_randomSeed);
    
    // Math utilities
    lua_register(L, "map", l_map);
    lua_register(L, "constrain", l_constrain);
    
    // Print functions
    lua_register(L, "print", l_print);
    lua_register(L, "println", l_println);
    
    // Helper functions
    lua_register(L, "typeof", l_typeof);
    lua_register(L, "tostring", l_tostring);
    lua_register(L, "tonumber", l_tonumber);
    lua_register(L, "len", l_len);
    lua_register(L, "keys", l_keys);
    lua_register(L, "values", l_values);
    lua_register(L, "dump", l_dump);
    
    // Create Arduino constants table
    lua_newtable(L);
    
    // Pin modes
    lua_pushinteger(L, OUTPUT);
    lua_setfield(L, -2, "OUTPUT");
    lua_pushinteger(L, INPUT);
    lua_setfield(L, -2, "INPUT");
    lua_pushinteger(L, INPUT_PULLUP);
    lua_setfield(L, -2, "INPUT_PULLUP");
    #ifdef INPUT_PULLDOWN
    lua_pushinteger(L, INPUT_PULLDOWN);
    lua_setfield(L, -2, "INPUT_PULLDOWN");
    #endif
    
    // Digital values
    lua_pushinteger(L, HIGH);
    lua_setfield(L, -2, "HIGH");
    lua_pushinteger(L, LOW);
    lua_setfield(L, -2, "LOW");
    
    // Common pins for ESP32
    
    // Set as global Arduino table
    lua_setglobal(L, "Arduino");
}