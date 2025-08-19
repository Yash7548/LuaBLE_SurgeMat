#ifndef ARDUINO_BINDINGS_H
#define ARDUINO_BINDINGS_H

extern "C" {
    #include "luat_base.h"
    #include "lua.h"
    #include "lauxlib.h"
    #include "lualib.h"
   // #include "driver/uart.h"
}

// Register all Arduino bindings to Lua state
void registerArduinoBindings(lua_State* L);

// Individual function declarations
int l_millis(lua_State* L);
int l_micros(lua_State* L);
int l_delay(lua_State* L);
int l_delayMicroseconds(lua_State* L);

int l_pinMode(lua_State* L);
int l_digitalWrite(lua_State* L);
int l_digitalRead(lua_State* L);

int l_analogRead(lua_State* L);
int l_analogWrite(lua_State* L);
int l_analogReadResolution(lua_State* L);
int l_analogWriteResolution(lua_State* L);

int l_pulseIn(lua_State* L);
int l_pulseInLong(lua_State* L);

int l_tone(lua_State* L);
int l_noTone(lua_State* L);

int l_random(lua_State* L);
int l_randomSeed(lua_State* L);

int l_map(lua_State* L);
int l_constrain(lua_State* L);

int l_print(lua_State* L);
int l_println(lua_State* L);

// Helper functions
int l_typeof(lua_State* L);
int l_tostring(lua_State* L);
int l_tonumber(lua_State* L);
int l_len(lua_State* L);
int l_keys(lua_State* L);
int l_values(lua_State* L);
int l_dump(lua_State* L);

#endif // ARDUINO_BINDINGS_H