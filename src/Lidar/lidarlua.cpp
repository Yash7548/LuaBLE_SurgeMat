#include "lidar.h"
#include "Lidar/lidarlua.h"
#include "Global/global.h"
 int max_dist=8190;
void lua_register_lidar(lua_State *L)
{
    lua_register(L, "lidar_top_readDisFlux", lua_wrapper_lidar_top_readDisFlux);
    lua_register(L, "lidar_bottom_readDisFlux", lua_wrapper_lidar_bottom_readDisFlux);
}


static int lua_wrapper_lidar_top_readDisFlux(lua_State *lua_state) {
    int16_t distance, flux;
    bool result = lidarTop.readDisFlux(distance, flux);

    if(distance==0)
    {
        distance=max_dist;
    }

    lua_pushnumber(lua_state, distance);
    lua_pushnumber(lua_state, flux);
    lua_pushboolean(lua_state, result);
    return 3;
}

static int lua_wrapper_lidar_bottom_readDisFlux(lua_State *lua_state) {
    int16_t distance, flux;
    bool result = lidarBottom.readDisFlux(distance, flux);

    if(distance==0)
    {
        distance=max_dist;
    }

    lua_pushnumber(lua_state, distance);
    lua_pushnumber(lua_state, flux);
    lua_pushboolean(lua_state, result);
    return 3;
}

