#include "display19.h"



// Constants for battery display
#define BATT_X 250       // X position of battery icon (right corner)
#define BATT_Y 10        // Y position of battery icon (top)
#define BATT_WIDTH 50    // Width of battery icon
#define BATT_HEIGHT 24   // Height of battery icon
#define BATT_BORDER 2    // Border thickness of battery icon
#define BATT_TIP_WIDTH 4 // Width of battery tip
#define BATT_TIP_HEIGHT 10 // Height of battery tip

// Battery voltage thresholds
#define BATT_MAX_VOLTAGE 4100  // 100% at 4.2V
#define BATT_MIN_VOLTAGE 3200  // 0% at 3.2V

// Colors
#define BATT_BORDER_COLOR TFT_WHITE
#define BATT_CRITICAL_COLOR TFT_RED
#define BATT_LOW_COLOR TFT_YELLOW
#define BATT_GOOD_COLOR TFT_GREEN
#define BATT_TEXT_COLOR TFT_WHITE
#define BATT_BG_COLOR TFT_BLACK


int prevBatteryPercentage = -1;
bool prevChargingState = false;

void lua_register_display19(lua_State *L)
{

    // Basic Display Functions
    lua_register(L, "clear_display", lua_wrapper_clear_display);
    lua_register(L, "display_set_cursor", lua_wrapper_display_setCursor);
    lua_register(L, "display_text_height", lua_wrapper_display_textHeight);
    lua_register(L, "display_print", lua_wrapper_display_print);
    lua_register(L, "display_show", lua_wrapper_display_show);

    // Drawing Functions
    lua_register(L, "display_draw_pixel", lua_wrapper_display_drawPixel);
    lua_register(L, "display_draw_line", lua_wrapper_display_drawLine);
    lua_register(L, "display_draw_rect", lua_wrapper_display_drawRect);
    lua_register(L, "display_fill_rect", lua_wrapper_display_fillRect);
    lua_register(L, "display_draw_circle", lua_wrapper_display_drawCircle);
    lua_register(L, "display_fill_circle", lua_wrapper_display_fillCircle);
    lua_register(L, "display_draw_triangle", lua_wrapper_display_drawTriangle);
    lua_register(L, "display_fill_triangle", lua_wrapper_display_fillTriangle);

    // Text Properties
    lua_register(L, "display_set_text_color", lua_wrapper_display_setTextColor);
    lua_register(L, "display_set_text_wrap", lua_wrapper_display_setTextWrap);
    lua_register(L, "display_set_rotation", lua_wrapper_display_setRotation);

    // Additional Display Functions
    lua_register(L, "display_set_brightness", lua_wrapper_display_setBrightness);
    lua_register(L, "display_fill_screen", lua_wrapper_display_fillScreen);
    lua_register(L, "display_draw_string", lua_wrapper_display_drawString);
    lua_register(L, "display_draw_number", lua_wrapper_display_drawNumber);
    lua_register(L, "display_draw_float", lua_wrapper_display_drawFloat);

    // Color Functions
    lua_register(L, "color565", lua_wrapper_display_color565);
    lua_register(L, "colors", lua_wrapper_display_get_colors);
    lua_register(L, "render", lua_render_jpeg);
    lua_register(L, "get_size", lua_get_jpeg_size);
}

// display ---------------------------------------------------------------------------------------------------------
// Display wrapper functions for TFT
static int lua_wrapper_clear_display(lua_State *lua_state)
{
    tft.fillScreen(TFT_BLACK); // Clear with black background
    return 0;
}

static int lua_wrapper_display_setCursor(lua_State *lua_state)
{
    int16_t x = luaL_checknumber(lua_state, 1);
    int16_t y = luaL_checknumber(lua_state, 2);
    tft.setCursor(x, y);
    return 0;
}

static int lua_wrapper_display_print(lua_State *lua_state)
{
    const char *text = luaL_checkstring(lua_state, 1);
    tft.print(text);
    return 0;
}

static int lua_wrapper_display_textHeight(lua_State *lua_state)
{
    uint8_t size = luaL_checknumber(lua_state, 1);
    tft.setTextSize(size);
    return 0;
}

static int lua_wrapper_display_show(lua_State *lua_state)
{
    // Most TFT displays don't need explicit show/display call
    return 0;
}

static int lua_wrapper_display_drawPixel(lua_State *lua_state)
{
    int16_t x = luaL_checknumber(lua_state, 1);
    int16_t y = luaL_checknumber(lua_state, 2);
    uint16_t color = luaL_checknumber(lua_state, 3);
    tft.drawPixel(x, y, color);
    return 0;
}

static int lua_wrapper_display_drawLine(lua_State *lua_state)
{
    int16_t x0 = luaL_checknumber(lua_state, 1);
    int16_t y0 = luaL_checknumber(lua_state, 2);
    int16_t x1 = luaL_checknumber(lua_state, 3);
    int16_t y1 = luaL_checknumber(lua_state, 4);
    uint16_t color = luaL_checknumber(lua_state, 5);
    tft.drawLine(x0, y0, x1, y1, color);
    return 0;
}

static int lua_wrapper_display_drawRect(lua_State *lua_state)
{
    int16_t x = luaL_checknumber(lua_state, 1);
    int16_t y = luaL_checknumber(lua_state, 2);
    int16_t w = luaL_checknumber(lua_state, 3);
    int16_t h = luaL_checknumber(lua_state, 4);
    uint16_t color = luaL_checknumber(lua_state, 5);
    tft.drawRect(x, y, w, h, color);
    return 0;
}

static int lua_wrapper_display_fillRect(lua_State *lua_state)
{
    int16_t x = luaL_checknumber(lua_state, 1);
    int16_t y = luaL_checknumber(lua_state, 2);
    int16_t w = luaL_checknumber(lua_state, 3);
    int16_t h = luaL_checknumber(lua_state, 4);
    uint16_t color = luaL_checknumber(lua_state, 5);
    tft.fillRect(x, y, w, h, color);
    return 0;
}

static int lua_wrapper_display_drawCircle(lua_State *lua_state)
{
    int16_t x = luaL_checknumber(lua_state, 1);
    int16_t y = luaL_checknumber(lua_state, 2);
    int16_t r = luaL_checknumber(lua_state, 3);
    uint16_t color = luaL_checknumber(lua_state, 4);
    tft.drawCircle(x, y, r, color);
    return 0;
}

static int lua_wrapper_display_fillCircle(lua_State *lua_state)
{
    int16_t x = luaL_checknumber(lua_state, 1);
    int16_t y = luaL_checknumber(lua_state, 2);
    int16_t r = luaL_checknumber(lua_state, 3);
    uint16_t color = luaL_checknumber(lua_state, 4);
    tft.fillCircle(x, y, r, color);
    return 0;
}

static int lua_wrapper_display_drawTriangle(lua_State *lua_state)
{
    int16_t x0 = luaL_checknumber(lua_state, 1);
    int16_t y0 = luaL_checknumber(lua_state, 2);
    int16_t x1 = luaL_checknumber(lua_state, 3);
    int16_t y1 = luaL_checknumber(lua_state, 4);
    int16_t x2 = luaL_checknumber(lua_state, 5);
    int16_t y2 = luaL_checknumber(lua_state, 6);
    uint16_t color = luaL_checknumber(lua_state, 7);
    tft.drawTriangle(x0, y0, x1, y1, x2, y2, color);
    return 0;
}

static int lua_wrapper_display_fillTriangle(lua_State *lua_state)
{
    int16_t x0 = luaL_checknumber(lua_state, 1);
    int16_t y0 = luaL_checknumber(lua_state, 2);
    int16_t x1 = luaL_checknumber(lua_state, 3);
    int16_t y1 = luaL_checknumber(lua_state, 4);
    int16_t x2 = luaL_checknumber(lua_state, 5);
    int16_t y2 = luaL_checknumber(lua_state, 6);
    uint16_t color = luaL_checknumber(lua_state, 7);
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, color);
    return 0;
}

static int lua_wrapper_display_setTextColor(lua_State *lua_state)
{
    uint16_t fgcolor = luaL_checknumber(lua_state, 1);
    if (lua_gettop(lua_state) > 1)
    {
        uint16_t bgcolor = luaL_checknumber(lua_state, 2);
        tft.setTextColor(fgcolor, bgcolor);
    }
    else
    {
        tft.setTextColor(fgcolor);
    }
    return 0;
}

static int lua_wrapper_display_setTextWrap(lua_State *lua_state)
{
    bool wrap = lua_toboolean(lua_state, 1);
    tft.setTextWrap(wrap);
    return 0;
}

static int lua_wrapper_display_setRotation(lua_State *lua_state)
{
    uint8_t r = luaL_checknumber(lua_state, 1);
    tft.setRotation(r);
    return 0;
}

static int lua_wrapper_display_setBrightness(lua_State *lua_state)
{
    // Most TFT displays don't have direct brightness control
    // You might need to implement PWM control if your hardware supports it
    return 0;
}

static int lua_wrapper_display_fillScreen(lua_State *lua_state)
{
    uint16_t color = luaL_checknumber(lua_state, 1);
    tft.fillScreen(color);
    return 0;
}

static int lua_wrapper_display_drawString(lua_State *lua_state)
{
    const char *str = luaL_checkstring(lua_state, 1);
    int32_t x = luaL_checknumber(lua_state, 2);
    int32_t y = luaL_checknumber(lua_state, 3);
    tft.setCursor(x, y);
    tft.print(str);
    return 0;
}

static int lua_wrapper_display_drawNumber(lua_State *lua_state)
{
    long num = luaL_checknumber(lua_state, 1);
    int32_t x = luaL_checknumber(lua_state, 2);
    int32_t y = luaL_checknumber(lua_state, 3);
    tft.setCursor(x, y);
    tft.print(num);
    return 0;
}

static int lua_wrapper_display_drawFloat(lua_State *lua_state)
{
    float num = luaL_checknumber(lua_state, 1);
    uint8_t decimal = luaL_checknumber(lua_state, 2);
    int32_t x = luaL_checknumber(lua_state, 3);
    int32_t y = luaL_checknumber(lua_state, 4);
    tft.setCursor(x, y);
    tft.print(num, decimal);
    return 0;
}

static int lua_wrapper_display_color565(lua_State *lua_state)
{
    uint8_t r = luaL_checkinteger(lua_state, 1);
    uint8_t g = luaL_checkinteger(lua_state, 2);
    uint8_t b = luaL_checkinteger(lua_state, 3);
    uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    lua_pushinteger(lua_state, color);
    return 1;
}

static int lua_wrapper_display_get_colors(lua_State *lua_state)
{
    lua_createtable(lua_state, 0, 16);

    // Basic colors in 565 format
    lua_pushstring(lua_state, "BLACK");
    lua_pushinteger(lua_state, 0x0000);
    lua_settable(lua_state, -3);

    lua_pushstring(lua_state, "WHITE");
    lua_pushinteger(lua_state, 0xFFFF);
    lua_settable(lua_state, -3);

    lua_pushstring(lua_state, "RED");
    lua_pushinteger(lua_state, 0xF800);
    lua_settable(lua_state, -3);

    lua_pushstring(lua_state, "GREEN");
    lua_pushinteger(lua_state, 0x07E0);
    lua_settable(lua_state, -3);

    lua_pushstring(lua_state, "BLUE");
    lua_pushinteger(lua_state, 0x001F);
    lua_settable(lua_state, -3);

    lua_pushstring(lua_state, "YELLOW");
    lua_pushinteger(lua_state, 0xFFE0);
    lua_settable(lua_state, -3);

    lua_pushstring(lua_state, "MAGENTA");
    lua_pushinteger(lua_state, 0xF81F);
    lua_settable(lua_state, -3);

    lua_pushstring(lua_state, "CYAN");
    lua_pushinteger(lua_state, 0x07FF);
    lua_settable(lua_state, -3);

    return 1;
}

// Function to be called from Lua to render JPEG
static int lua_render_jpeg(lua_State *L)
{
    // Check if we received a string parameter
    const char *path = luaL_checkstring(L, 1);
    if (path == NULL)
    {
        lua_pushboolean(L, 0); // Return false to Lua
        lua_pushstring(L, "Path argument required");
        return 2; // Return two values (bool and error message)
    }

    // Static initialization of decoder if needed
    static bool decoder_initialized = false;
    if (!decoder_initialized)
    {
        TJpgDec.setSwapBytes(true);
        TJpgDec.setCallback(tft_output);
        decoder_initialized = true;
    }

    // Try to render the JPEG
    bool success = TJpgDec.drawFsJpg(0, 0, path);

    // Return success/failure to Lua
    lua_pushboolean(L, success);
    if (!success)
    {
        lua_pushstring(L, "Failed to render JPEG");
        return 2; // Return both values
    }
    return 1; // Just return success
}

// Optional: Function to get image dimensions
static int lua_get_jpeg_size(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    if (path == NULL)
    {
        lua_pushnil(L);
        lua_pushstring(L, "Path argument required");
        return 2;
    }

    uint16_t width = 0, height = 0;
    if (!TJpgDec.getFsJpgSize(&width, &height, path))
    {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to get image dimensions");
        return 2;
    }

    // Return width and height as a table
    lua_createtable(L, 0, 2);
    lua_pushinteger(L, width);
    lua_setfield(L, -2, "width");
    lua_pushinteger(L, height);
    lua_setfield(L, -2, "height");
    return 1;
}
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
   // Stop further decoding as image is running off bottom of screen
  if ( y >= tft.height() ) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);

  // This might work instead if you adapt the sketch to use the Adafruit_GFX library
  // tft.drawRGBBitmap(x, y, bitmap, w, h);

  // Return 1 to decode next block
  return 1;
}

bool renderJPEG(const char *path)
{
    static bool initialized = false;

    // One-time initialization
    if (!initialized)
    {
        // Set up the JPEG decoder
        TJpgDec.setSwapBytes(true);
        TJpgDec.setCallback(tft_output);
        initialized = true;
    }
  uint16_t w = 0, h = 0;
  TJpgDec.getFsJpgSize(&w, &h, path); // Note name preceded with "/"
  Serial.print("Width = "); Serial.print(w); Serial.print(", height = "); Serial.println(h);
    // Draw the image
    return TJpgDec.drawFsJpg(0, 0, path);
}


// void displayBatteryStatus(TFT_eSPI &tft, uint16_t batteryVoltage, bool isCharging) {
//     // Calculate battery percentage
//     int batteryPercentage;
//     if (batteryVoltage >= BATT_MAX_VOLTAGE) {
//       batteryPercentage = 100;
//     } else if (batteryVoltage <= BATT_MIN_VOLTAGE) {
//       batteryPercentage = 0;
//     } else {
//       batteryPercentage = map(batteryVoltage, BATT_MIN_VOLTAGE, BATT_MAX_VOLTAGE, 0, 100);
//     }
    
//     // Only update display if percentage changed or charging state changed
//     if (batteryPercentage != prevBatteryPercentage || isCharging != prevChargingState) {
//       // Store current state
//       prevBatteryPercentage = batteryPercentage;
//       prevChargingState = isCharging;
      
//       // Clear battery area
//       tft.fillRect(BATT_X - 10, BATT_Y - 5, BATT_WIDTH + 20, BATT_HEIGHT + 10, BATT_BG_COLOR);
      
//       // Draw battery outline
//       tft.fillRoundRect(BATT_X, BATT_Y, BATT_WIDTH, BATT_HEIGHT, 3, BATT_BORDER_COLOR);
//       tft.fillRoundRect(BATT_X + BATT_BORDER, BATT_Y + BATT_BORDER,BATT_WIDTH - (2 * BATT_BORDER), BATT_HEIGHT - (2 * BATT_BORDER), 2, BATT_BG_COLOR);
                        
//       // Draw battery tip (positive terminal)
//       tft.fillRect(BATT_X + BATT_WIDTH,BATT_Y + (BATT_HEIGHT - BATT_TIP_HEIGHT) / 2, BATT_TIP_WIDTH, BATT_TIP_HEIGHT, BATT_BORDER_COLOR);
      
//       // Choose color based on battery level
//       uint16_t fillColor;
//       if (batteryPercentage <= 15) {
//         fillColor = BATT_CRITICAL_COLOR;
//       } else if (batteryPercentage <= 30) {
//         fillColor = BATT_LOW_COLOR;
//       } else {
//         fillColor = BATT_GOOD_COLOR;
//       }
      
//       // Calculate fill width based on percentage
//       int fillWidth = ((BATT_WIDTH - (2 * BATT_BORDER)) * batteryPercentage) / 100;
      
//       // Draw battery fill level
//       if (fillWidth > 0) {
//         tft.fillRoundRect(BATT_X + BATT_BORDER, BATT_Y + BATT_BORDER, 
//                         fillWidth, BATT_HEIGHT - (2 * BATT_BORDER), 
//                         2, fillColor);
//       }
      
//       // Draw charging indicator if charging
//       if (isCharging) {
//         // Lightning bolt symbol or text
//         tft.setTextColor(TFT_YELLOW, BATT_BG_COLOR);
//         tft.setTextSize(1);
//         tft.setCursor(BATT_X - 10, BATT_Y + 8);
//         tft.print("+");
//       }
      
//       // Display percentage text
//       tft.setTextColor(BATT_TEXT_COLOR);
//       tft.setTextSize(1);
//       tft.setCursor(BATT_X + BATT_WIDTH / 2 - 10, BATT_Y + BATT_HEIGHT/2 - 4);
//       tft.print(batteryPercentage);
//       tft.print("%");
//     }
//   }


// Constants for lidar display
#define LIDAR_X 10         // X position of lidar readings (left side)
#define LIDAR_Y 210        // Y position adjusted to accommodate taller height
#define LIDAR_HEIGHT 20    // Height increased to 20px as requested
#define LIDAR_BG_COLOR TFT_BLACK
#define LIDAR_TEXT_COLOR TFT_WHITE
#define LIDAR_TOP_COLOR TFT_CYAN
#define LIDAR_BOTTOM_COLOR TFT_GREEN

// Variables to track previous readings
int prevLidarTop = -1;
int prevLidarBottom = -1;
unsigned long lastLidarDisplayTime = 0;

void displayLidarReadings(int lidarTop, int lidarBottom) {
  unsigned long currentTime = millis();
  
  // Only update if readings changed or 100ms passed since last update
  if (lidarTop != prevLidarTop || lidarBottom != prevLidarBottom || 
      (currentTime - lastLidarDisplayTime >= 100)) {
    
    // Store current readings and time
    prevLidarTop = lidarTop;
    prevLidarBottom = lidarBottom;
    lastLidarDisplayTime = currentTime;
    
    // Clear lidar display area (without affecting battery area)
    tft.fillRect(0, LIDAR_Y, 320, LIDAR_HEIGHT, LIDAR_BG_COLOR);
    
    // Display top reading (left side)
    tft.setTextSize(2);
    tft.setCursor(LIDAR_X, LIDAR_Y + 5); // Centered vertically in the 20px space
    tft.setTextColor(LIDAR_TOP_COLOR);
    tft.print("Top: ");
    tft.print(lidarTop);
   // tft.print(" mm");
    
    // Display bottom reading (right side)
    tft.setCursor(160, LIDAR_Y + 5); // Centered vertically in the 20px space
    tft.setTextColor(LIDAR_BOTTOM_COLOR);
    tft.print("Bottom: ");
    tft.print(lidarBottom);
    //tft.print(" mm");
  }
}