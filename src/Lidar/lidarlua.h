#ifndef LIDARLUA_H
#define LIDARLUA_H

#include "Global/global.h"
#include "lidar.h"


void lua_register_lidar(lua_State *L);
static int lua_wrapper_lidar_top_readDisFlux(lua_State *lua_state);
static int lua_wrapper_lidar_bottom_readDisFlux(lua_State *lua_state);


#endif