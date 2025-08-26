#ifndef Forcesensor_Lua_H
#define Forcesensor_Lua_H

#include "Global/global.h"
#include "ForceSensor.h"

// Function to register all Force Sensor Lua functions
void lua_register_forcesensor(lua_State *L);

// Left Force sensor functions
static int lua_wrapper_force_sensor_left_read(lua_State *lua_state);
static int lua_wrapper_force_sensor_left_read_average(lua_State *lua_state);
static int lua_wrapper_force_sensor_left_read_filtered(lua_State *lua_state);
static int lua_wrapper_force_sensor_left_read_mapped(lua_State *lua_state);

// Right Force sensor functions
static int lua_wrapper_force_sensor_right_read(lua_State *lua_state);
static int lua_wrapper_force_sensor_right_read_average(lua_State *lua_state);
static int lua_wrapper_force_sensor_right_read_filtered(lua_State *lua_state);
static int lua_wrapper_force_sensor_right_read_mapped(lua_State *lua_state);

// Combined functions for both sensors
static int lua_wrapper_force_sensor_both_read(lua_State *lua_state);
static int lua_wrapper_force_sensor_both_read_average(lua_State *lua_state);
static int lua_wrapper_force_sensor_both_read_filtered(lua_State *lua_state);

#endif