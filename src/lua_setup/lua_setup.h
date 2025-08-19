#ifndef LUA_H
#define LUA_H

#include "config.h"
#include "luatoswrapper.h"
#include "Global/global.h"

#include "require_lua.h"

// BLE connection state
extern bool bleConnected;

// Setup and main functions
void lua_setup();
void lua_loop(String script);
void luaClose(String stopScript);

// BLE handlers
void handleBleConnect();
void handleBleDisconnect();

#endif