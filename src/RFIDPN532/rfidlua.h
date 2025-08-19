#ifndef RFIDLUA_H
#define RFIDLUA_H
#include "Global/global.h"
#include "RFIDPN532/rfid_manager.h"

void lua_register_rfid(lua_State *L);

static int lua_wrapper_nfc_read_passive_target_id(lua_State *lua_state);
static int lua_wrapper_rfid_start_reading(lua_State *lua_state);
static int lua_wrapper_rfid_stop_reading(lua_State *lua_state);
static int lua_wrapper_rfid_is_reading(lua_State *lua_state);
static int lua_wrapper_rfid_read(lua_State *lua_state);


#endif