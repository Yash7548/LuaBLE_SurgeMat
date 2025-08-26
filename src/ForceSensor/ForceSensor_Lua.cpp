#include "ForceSensor.h"
#include "ForceSensor_Lua.h"
#include "Global/global.h"

void lua_register_forcesensor(lua_State *L)
{
    // Left force sensor functions
    lua_register(L, "force_sensor_left_read", lua_wrapper_force_sensor_left_read);
    lua_register(L, "force_sensor_left_read_average", lua_wrapper_force_sensor_left_read_average);
    lua_register(L, "force_sensor_left_read_filtered", lua_wrapper_force_sensor_left_read_filtered);
    lua_register(L, "force_sensor_left_read_mapped", lua_wrapper_force_sensor_left_read_mapped);

    // Right force sensor functions
    lua_register(L, "force_sensor_right_read", lua_wrapper_force_sensor_right_read);
    lua_register(L, "force_sensor_right_read_average", lua_wrapper_force_sensor_right_read_average);
    lua_register(L, "force_sensor_right_read_filtered", lua_wrapper_force_sensor_right_read_filtered);
    lua_register(L, "force_sensor_right_read_mapped", lua_wrapper_force_sensor_right_read_mapped);

    // Combined functions for both sensors
    lua_register(L, "force_sensor_both_read", lua_wrapper_force_sensor_both_read);
    lua_register(L, "force_sensor_both_read_average", lua_wrapper_force_sensor_both_read_average);
    lua_register(L, "force_sensor_both_read_filtered", lua_wrapper_force_sensor_both_read_filtered);

    LLOGI("Force Sensor Lua functions registered");
}

// LEFT FORCE SENSOR FUNCTIONS
static int lua_wrapper_force_sensor_left_read(lua_State *lua_state)
{
    int value = forceSensorLeft.read();
    lua_pushinteger(lua_state, value);
    return 1;
}

static int lua_wrapper_force_sensor_left_read_average(lua_State *lua_state)
{
    float value = forceSensorLeft.readAverage();
    lua_pushnumber(lua_state, value);
    return 1;
}

static int lua_wrapper_force_sensor_left_read_filtered(lua_State *lua_state)
{
    float value = forceSensorLeft.readFiltered();
    lua_pushnumber(lua_state, value);
    return 1;
}

static int lua_wrapper_force_sensor_left_read_mapped(lua_State *lua_state)
{
    float fromLow = luaL_checknumber(lua_state, 1);
    float fromHigh = luaL_checknumber(lua_state, 2);
    float toLow = luaL_checknumber(lua_state, 3);
    float toHigh = luaL_checknumber(lua_state, 4);
    
    float value = forceSensorLeft.readMapped(fromLow, fromHigh, toLow, toHigh);
    lua_pushnumber(lua_state, value);
    return 1;
}

// RIGHT FORCE SENSOR FUNCTIONS
static int lua_wrapper_force_sensor_right_read(lua_State *lua_state)
{
    int value = forceSensorRight.read();
    lua_pushinteger(lua_state, value);
    return 1;
}

static int lua_wrapper_force_sensor_right_read_average(lua_State *lua_state)
{
    float value = forceSensorRight.readAverage();
    lua_pushnumber(lua_state, value);
    return 1;
}

static int lua_wrapper_force_sensor_right_read_filtered(lua_State *lua_state)
{
    float value = forceSensorRight.readFiltered();
    lua_pushnumber(lua_state, value);
    return 1;
}

static int lua_wrapper_force_sensor_right_read_mapped(lua_State *lua_state)
{
    float fromLow = luaL_checknumber(lua_state, 1);
    float fromHigh = luaL_checknumber(lua_state, 2);
    float toLow = luaL_checknumber(lua_state, 3);
    float toHigh = luaL_checknumber(lua_state, 4);
    
    float value = forceSensorRight.readMapped(fromLow, fromHigh, toLow, toHigh);
    lua_pushnumber(lua_state, value);
    return 1;
}

// COMBINED FUNCTIONS FOR BOTH SENSORS
static int lua_wrapper_force_sensor_both_read(lua_State *lua_state)
{
    int leftValue = forceSensorLeft.read();
    int rightValue = forceSensorRight.read();
    
    lua_pushinteger(lua_state, leftValue);   // First return value (left)
    lua_pushinteger(lua_state, rightValue);  // Second return value (right)
    return 2; // Return 2 values
}

static int lua_wrapper_force_sensor_both_read_average(lua_State *lua_state)
{
    float leftValue = forceSensorLeft.readAverage();
    float rightValue = forceSensorRight.readAverage();
    
    lua_pushnumber(lua_state, leftValue);   // First return value (left)
    lua_pushnumber(lua_state, rightValue);  // Second return value (right)
    return 2; // Return 2 values
}

static int lua_wrapper_force_sensor_both_read_filtered(lua_State *lua_state)
{
    float leftValue = forceSensorLeft.readFiltered();
    float rightValue = forceSensorRight.readFiltered();
    
    lua_pushnumber(lua_state, leftValue);   // First return value (left)
    lua_pushnumber(lua_state, rightValue);  // Second return value (right)
    return 2; // Return 2 values
}