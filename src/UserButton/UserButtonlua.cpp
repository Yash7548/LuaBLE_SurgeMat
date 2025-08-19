/**
 * @file UserButtonLua.cpp
 * @brief Implementation of Lua wrapper for UserButton class
 */

#include "UserButtonLua.h"

UserButton *g_buttonInstance = nullptr;
void setButtonInstance(UserButton *instance)
{
    g_buttonInstance = instance;
}
void lua_register_userbutton(lua_State *L)
{
    const luaL_Reg buttonLib[] = {
        // Non-blocking state queries
        {"is_pressed", lua_wrapper_button_is_pressed},
        {"is_longpress", lua_wrapper_button_is_longpress},
        {"get_click_count", lua_wrapper_button_get_click_count},

        // Event checks
        {"was_clicked", lua_wrapper_button_was_clicked},
        {"was_double_clicked", lua_wrapper_button_was_double_clicked},
        {"was_long_pressed", lua_wrapper_button_was_long_pressed},
        {"was_multi_clicked", lua_wrapper_button_was_multi_clicked},

        // Blocking wait functions
        {"wait_click", lua_wrapper_button_wait_click},
        {"wait_double_click", lua_wrapper_button_wait_double_click},
        {"wait_long_press", lua_wrapper_button_wait_long_press},
        {"wait_multi_click", lua_wrapper_button_wait_multi_click},

        // Configuration
        {"set_longpress_time", lua_wrapper_button_set_longpress_time},
        {"set_doubleclick_time", lua_wrapper_button_set_doubleclick_time},
        {"set_multiclick_time", lua_wrapper_button_set_multiclick_time},
        {"set_max_multiclicks", lua_wrapper_button_set_max_multiclicks},

        {NULL, NULL}};

    luaL_newlib(L, buttonLib);
    lua_setglobal(L, "button");
}

// Non-blocking state queries
int lua_wrapper_button_is_pressed(lua_State *L)
{
    if (!g_buttonInstance)
        return luaL_error(L, "Button not initialized");
    lua_pushboolean(L, g_buttonInstance->getButtonState().isPressed);
    return 1;
}

int lua_wrapper_button_is_longpress(lua_State *L)
{
    if (!g_buttonInstance)
        return luaL_error(L, "Button not initialized");
    lua_pushboolean(L, g_buttonInstance->getButtonState().isLongPress);
    return 1;
}

int lua_wrapper_button_get_click_count(lua_State *L)
{
    if (!g_buttonInstance)
        return luaL_error(L, "Button not initialized");
    lua_pushinteger(L, g_buttonInstance->getButtonState().clickCount);
    return 1;
}

// Event checks with auto-clear
int lua_wrapper_button_was_clicked(lua_State *L)
{
    if (!g_buttonInstance)
        return luaL_error(L, "Button not initialized");
    bool clicked = g_buttonInstance->getButtonEvents().click;
    if (clicked)
        g_buttonInstance->clearEvents();
    lua_pushboolean(L, clicked);
    return 1;
}

int lua_wrapper_button_was_double_clicked(lua_State *L)
{
    if (!g_buttonInstance)
        return luaL_error(L, "Button not initialized");
    bool doubleClicked = g_buttonInstance->getButtonEvents().doubleClick;
    if (doubleClicked)
        g_buttonInstance->clearEvents();
    lua_pushboolean(L, doubleClicked);
    return 1;
}

int lua_wrapper_button_was_long_pressed(lua_State *L)
{
    if (!g_buttonInstance)
        return luaL_error(L, "Button not initialized");
    bool longPressed = g_buttonInstance->getButtonEvents().longPress;
    if (longPressed)
        g_buttonInstance->clearEvents();
    lua_pushboolean(L, longPressed);
    return 1;
}

int lua_wrapper_button_was_multi_clicked(lua_State *L)
{
    if (!g_buttonInstance)
        return luaL_error(L, "Button not initialized");
    bool multiClicked = g_buttonInstance->getButtonEvents().multiClick;
    if (multiClicked)
        g_buttonInstance->clearEvents();
    lua_pushboolean(L, multiClicked);
    return 1;
}

// Blocking wait functions
int lua_wrapper_button_wait_click(lua_State *L)
{
    if (!g_buttonInstance)
        return luaL_error(L, "Button not initialized");

    int32_t timeout = TimeoutHelper::getTimeout(L, 1);
    uint32_t startTime = millis();

    while (!g_buttonInstance->getButtonEvents().click)
    {
        g_buttonInstance->tick();
        if (TimeoutHelper::hasTimedOut(startTime, timeout))
        {
            lua_pushboolean(L, false);
            return 1;
        }
        delay(1);
    }

    g_buttonInstance->clearEvents();
    lua_pushboolean(L, true);
    return 1;
}

int lua_wrapper_button_wait_double_click(lua_State *L)
{
    if (!g_buttonInstance)
        return luaL_error(L, "Button not initialized");

    int32_t timeout = TimeoutHelper::getTimeout(L, 1);
    uint32_t startTime = millis();

    while (!g_buttonInstance->getButtonEvents().doubleClick)
    {
        g_buttonInstance->tick();
        if (TimeoutHelper::hasTimedOut(startTime, timeout))
        {
            lua_pushboolean(L, false);
            return 1;
        }
        delay(1);
    }

    g_buttonInstance->clearEvents();
    lua_pushboolean(L, true);
    return 1;
}

int lua_wrapper_button_wait_long_press(lua_State *L)
{
    if (!g_buttonInstance)
        return luaL_error(L, "Button not initialized");

    int32_t timeout = TimeoutHelper::getTimeout(L, 1);
    uint32_t startTime = millis();

    while (!g_buttonInstance->getButtonEvents().longPress)
    {
        g_buttonInstance->tick();
        if (TimeoutHelper::hasTimedOut(startTime, timeout))
        {
            lua_pushboolean(L, false);
            return 1;
        }
        delay(1);
    }

    g_buttonInstance->clearEvents();
    lua_pushboolean(L, true);
    return 1;
}

int lua_wrapper_button_wait_multi_click(lua_State *L)
{
    if (!g_buttonInstance)
        return luaL_error(L, "Button not initialized");

    int32_t timeout = TimeoutHelper::getTimeout(L, 1);
    uint32_t startTime = millis();

    while (!g_buttonInstance->getButtonEvents().multiClick)
    {
        g_buttonInstance->tick();
        if (TimeoutHelper::hasTimedOut(startTime, timeout))
        {
            lua_pushboolean(L, false);
            return 1;
        }
        delay(1);
    }

    g_buttonInstance->clearEvents();
    lua_pushboolean(L, true);
    return 1;
}

// Configuration functions
int lua_wrapper_button_set_longpress_time(lua_State *L)
{
    if (!g_buttonInstance)
        return luaL_error(L, "Button not initialized");
    uint32_t ms = static_cast<uint32_t>(luaL_checknumber(L, 1));
    g_buttonInstance->setLongPressTime(ms);
    return 0;
}

int lua_wrapper_button_set_doubleclick_time(lua_State *L)
{
    if (!g_buttonInstance)
        return luaL_error(L, "Button not initialized");
    uint32_t ms = static_cast<uint32_t>(luaL_checknumber(L, 1));
    g_buttonInstance->setDoubleClickTime(ms);
    return 0;
}

int lua_wrapper_button_set_multiclick_time(lua_State *L)
{
    if (!g_buttonInstance)
        return luaL_error(L, "Button not initialized");
    uint32_t ms = static_cast<uint32_t>(luaL_checknumber(L, 1));
    g_buttonInstance->setMultiClickTime(ms);
    return 0;
}

int lua_wrapper_button_set_max_multiclicks(lua_State *L)
{
    if (!g_buttonInstance)
        return luaL_error(L, "Button not initialized");
    uint8_t count = static_cast<uint8_t>(luaL_checkinteger(L, 1));
    g_buttonInstance->setMaxMultiClicks(count);
    return 0;
}