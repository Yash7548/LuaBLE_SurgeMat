#ifndef DISPLAY19_H
#define DISPLAY19_H
#include "Global/global.h"

void lua_register_display19(lua_State *L);
bool renderJPEG(const char *path);
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
void displayBatteryStatus(TFT_eSPI &tft, uint16_t batteryVoltage, bool isCharging);
void displayLidarReadings(int lidarTop, int lidarBottom);

static int lua_render_jpeg(lua_State *lua_state);
static int lua_get_jpeg_size(lua_State *lua_state);
static int lua_wrapper_clear_display(lua_State *lua_state);
static int lua_wrapper_display_setCursor(lua_State *lua_state);
static int lua_wrapper_display_textHeight(lua_State *lua_state);
static int lua_wrapper_display_print(lua_State *lua_state);
static int lua_wrapper_display_show(lua_State *lua_state);

// Drawing Functions
static int lua_wrapper_display_drawPixel(lua_State *lua_state);
static int lua_wrapper_display_drawLine(lua_State *lua_state);
static int lua_wrapper_display_drawRect(lua_State *lua_state);
static int lua_wrapper_display_fillRect(lua_State *lua_state);
static int lua_wrapper_display_drawCircle(lua_State *lua_state);
static int lua_wrapper_display_fillCircle(lua_State *lua_state);
static int lua_wrapper_display_drawTriangle(lua_State *lua_state);
static int lua_wrapper_display_fillTriangle(lua_State *lua_state);

// Text Properties
static int lua_wrapper_display_setTextColor(lua_State *lua_state);
static int lua_wrapper_display_setTextWrap(lua_State *lua_state);
static int lua_wrapper_display_setRotation(lua_State *lua_state);

// Additional Display Functions
static int lua_wrapper_display_setBrightness(lua_State *lua_state);
static int lua_wrapper_display_fillScreen(lua_State *lua_state);
static int lua_wrapper_display_drawString(lua_State *lua_state);
static int lua_wrapper_display_drawNumber(lua_State *lua_state);
static int lua_wrapper_display_drawFloat(lua_State *lua_state);

// Color Functions
static int lua_wrapper_display_color565(lua_State *lua_state);
static int lua_wrapper_display_colorRGB(lua_State *lua_state);
static int lua_wrapper_display_get_colors(lua_State *lua_state);

#endif