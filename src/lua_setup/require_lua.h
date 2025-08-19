#ifndef REQUIRE_LUA_H
#define REQUIRE_LUA_H

#include "global/global.h"

#ifdef __cplusplus
extern "C" {
#endif

// Custom require function for SPIFFS (reads line by line)
int lua_custom_require(lua_State *L);

// Register the custom require function
void register_custom_require(lua_State *L);

// Helper function to check if file exists in SPIFFS
bool spiffs_file_exists(const char* filepath);

#ifdef __cplusplus
}
#endif

#endif // REQUIRE_LUA_H 