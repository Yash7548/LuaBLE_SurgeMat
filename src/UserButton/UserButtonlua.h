/**
 * @file UserButtonLua.h
 * @brief Lua wrapper interface for UserButton class
 */

#ifndef USER_BUTTON_LUA_H
#define USER_BUTTON_LUA_H

#include "UserButton.h"
#include "Global/global.h"  

// Global button instance for Lua integration
extern UserButton* g_buttonInstance;

// Lua wrapper registration function
void lua_register_userbutton(lua_State* L);

// Helper functions for timeout handling
struct TimeoutHelper {
    static bool hasTimedOut(uint32_t startTime, int32_t timeout) {
        if (timeout <= 0) return false;  // No timeout
        return (millis() - startTime) >= static_cast<uint32_t>(timeout);
    }
    
    static int32_t getTimeout(lua_State* L, int arg) {
        if (lua_isnoneornil(L, arg)) return 0;  // Unlimited timeout
        return static_cast<int32_t>(luaL_checknumber(L, arg));
    }
};
void setButtonInstance(UserButton* instance);


// Function signatures for Lua
extern "C" {
    // Non-blocking state queries
    int lua_wrapper_button_is_pressed(lua_State* L);
    int lua_wrapper_button_is_longpress(lua_State* L);
    int lua_wrapper_button_get_click_count(lua_State* L);
    
    // Event checks
    int lua_wrapper_button_was_clicked(lua_State* L);
    int lua_wrapper_button_was_double_clicked(lua_State* L);
    int lua_wrapper_button_was_long_pressed(lua_State* L);
    int lua_wrapper_button_was_multi_clicked(lua_State* L);
    
    // Blocking wait functions with timeout
    int lua_wrapper_button_wait_click(lua_State* L);
    int lua_wrapper_button_wait_double_click(lua_State* L);
    int lua_wrapper_button_wait_long_press(lua_State* L);
    int lua_wrapper_button_wait_multi_click(lua_State* L);
    
    // Configuration
    int lua_wrapper_button_set_longpress_time(lua_State* L);
    int lua_wrapper_button_set_doubleclick_time(lua_State* L);
    int lua_wrapper_button_set_multiclick_time(lua_State* L);
    int lua_wrapper_button_set_max_multiclicks(lua_State* L);
}

#endif // USER_BUTTON_LUA_H