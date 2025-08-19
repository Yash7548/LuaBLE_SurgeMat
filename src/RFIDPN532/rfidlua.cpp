#include "rfidlua.h"

void lua_register_rfid(lua_State *L)
{
    // rfid
    lua_register(L, "read_rfid", lua_wrapper_nfc_read_passive_target_id);
    lua_register(L, "rfid_start_reading", lua_wrapper_rfid_start_reading);
    lua_register(L, "rfid_stop_reading", lua_wrapper_rfid_stop_reading);
    lua_register(L, "rfid_is_reading", lua_wrapper_rfid_is_reading);
    lua_register(L, "rfid_read_data", lua_wrapper_rfid_read);
}

static int lua_wrapper_nfc_read_passive_target_id(lua_State *lua_state)
{
    // Get timeout from Lua (optional parameter, default 100ms)
    uint16_t timeout = luaL_optinteger(lua_state, 1, 100);

    uint8_t success;
    uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer for UID
    uint8_t uidLength;                     // Length of the UID (4 or 7 bytes)

    // Read passive target ID with timeout
    // success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, timeout);

    if (success)
    {
        // Create table to return multiple values
        lua_createtable(lua_state, 0, 2);

        // Add success status
        lua_pushstring(lua_state, "success");
        lua_pushboolean(lua_state, true);
        lua_settable(lua_state, -3);

        // Add UID as hex string
        String uidString = "";
        for (uint8_t i = 0; i < uidLength; i++)
        {
            if (uid[i] < 0x10)
            {
                uidString += "0"; // Add leading zero for single digit hex
            }
            uidString += String(uid[i], HEX);
        }

        lua_pushstring(lua_state, "uid");
        lua_pushstring(lua_state, uidString.c_str());
        lua_settable(lua_state, -3);

        // Add UID length
        lua_pushstring(lua_state, "length");
        lua_pushnumber(lua_state, uidLength);
        lua_settable(lua_state, -3);
    }
    else
    {
        // Create table with failure status
        lua_createtable(lua_state, 0, 1);
        lua_pushstring(lua_state, "success");
        lua_pushboolean(lua_state, false);
        lua_settable(lua_state, -3);
    }

    return 1; // Return one table
}
static int lua_wrapper_rfid_start_reading(lua_State *lua_state)
{
    digitalWrite(DISPLAY_CS, HIGH);
    digitalWrite(RFID_CS, LOW);
    digitalWrite(RFID_RST , LOW);
    delay(100);
    digitalWrite(RFID_RST , HIGH);

    // rfid.startReading();
    return 0;
}

static int lua_wrapper_rfid_stop_reading(lua_State *lua_state)
{
    digitalWrite(DISPLAY_CS, LOW);
    digitalWrite(RFID_CS, HIGH);
    // rfid.stopReading();
    return 0;
}

static int lua_wrapper_rfid_is_reading(lua_State *lua_state)
{
    bool isReading = rfid.isReading();
    lua_pushboolean(lua_state, isReading);
    return 1;
}

static int lua_wrapper_rfid_read(lua_State *lua_state)
{

    // rfid.startReading();

    // Read the data
    uint8_t buffer[8]; // Max UID length is 7 bytes + 1 for safety
    uint8_t length;

    // bool hasData = rfid.readData(buffer, &length);

    delay(100);
    nfc.begin();
    bool hasData = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, buffer, &length, 1000);
    Serial.println("readData: " + String(hasData) + " " + String(length));
    // Stop reading
    // rfid.stopReading();

    if (hasData)
    {
        // Convert UID to hex string
        String uidString = "";
        for (uint8_t i = 0; i < length; i++)
        {
            if (buffer[i] < 0x10)
            {
                uidString += "0"; // Add leading zero for single digit hex
            }
            uidString += String(buffer[i], HEX);
        }
        uidString.toLowerCase(); // Convert to lowercase for comparison

        // Check if length matches if specified
        // Push success status
        lua_pushboolean(lua_state, true);

        // Push the UID string
        lua_pushstring(lua_state, uidString.c_str());

        // Push the length
        lua_pushnumber(lua_state, length);

        return 3; // Return all three values
    }
    else
    {
        // No card detected
        lua_pushboolean(lua_state, false);
        lua_pushnil(lua_state);
        lua_pushnil(lua_state);
        return 3;
    }
}