#include "storage.h"
#include "config.h"
extern "C"
{
#define LUAT_LOG_TAG "STORAGE_CTRL"
#include "luat_log.h"
}
Storage::Storage(const char *preferencesName) : preferencesName(preferencesName) {}

Storage::~Storage()
{
    preferences.end();
}

void Storage::initPreferences()
{
    preferences.begin(preferencesName, false);
}

bool Storage::store(const String &key, const String &value)
{
    return preferences.putString(compressKey(key.c_str()).c_str(), value);
}

bool Storage::store(const String &key, int value)
{
    return preferences.putInt(compressKey(key.c_str()).c_str(), value);
}

bool Storage::store(const String &key, float value)
{
    return preferences.putFloat(compressKey(key.c_str()).c_str(), value);
}

bool Storage::store(const String &key, double value)
{
    return preferences.putDouble(compressKey(key.c_str()).c_str(), value);
}

bool Storage::store(const String &key, bool value)
{
    return preferences.putBool(compressKey(key.c_str()).c_str(), value);
}

String Storage::getString(const String &key, const String &defaultValue)
{
    return preferences.getString(compressKey(key.c_str()).c_str(), defaultValue);
}

int Storage::getInt(const String &key, int defaultValue)
{
    return preferences.getInt(compressKey(key.c_str()).c_str(), defaultValue);
}

float Storage::getFloat(const String &key, float defaultValue)
{
    return preferences.getFloat(compressKey(key.c_str()).c_str(), defaultValue);
}

double Storage::getDouble(const String &key, double defaultValue)
{
    return preferences.getDouble(compressKey(key.c_str()).c_str(), defaultValue);
}

bool Storage::getBool(const String &key, bool defaultValue)
{
    return preferences.getUChar(compressKey(key.c_str()).c_str(), defaultValue);
}

bool Storage::remove(const String &key)
{
    return preferences.remove(compressKey(key.c_str()).c_str());
}

void Storage::clear()
{
    preferences.clear();
}

void Storage::clearPreferences()
{
    preferences.clear();
}

String Storage::compressKey(const String &key)
{
    String compressedKey = key;
    if (key.length() > 15)
    {
        compressedKey = key.substring(0, 15);
    }
    return compressedKey;
}

String Storage::exportToJson(const String &jsonKeys)
{
    DynamicJsonDocument keysDoc(1024);
    DeserializationError error = deserializeJson(keysDoc, jsonKeys);
    if (error)
    {
        return "{}";
    }

    DynamicJsonDocument exportDoc(1024);
    preferences.begin(preferencesName, true);

    for (JsonVariant key : keysDoc.as<JsonArray>())
    {
        if (key.is<const char *>())
        {
            String keyStr = key.as<const char *>();
            PreferenceType type = preferences.getType(compressKey(keyStr).c_str());
            switch (type)
            {
            case PT_I8:
            case PT_U8:
                exportDoc[keyStr] = (bool)preferences.getUChar(compressKey(keyStr).c_str());
                break;
            case PT_I16:
            case PT_U16:
                exportDoc[keyStr] = preferences.getShort(compressKey(keyStr).c_str());
                break;
            case PT_I32:
            case PT_U32:
                exportDoc[keyStr] = preferences.getInt(compressKey(keyStr).c_str());
                break;
            case PT_I64:
            case PT_U64:
                exportDoc[keyStr] = preferences.getLong64(compressKey(keyStr).c_str());
                break;
            case PT_STR:
                exportDoc[keyStr] = preferences.getString(compressKey(keyStr).c_str());
                break;
            case PT_BLOB:
                exportDoc[keyStr] = preferences.getFloat(compressKey(keyStr).c_str());
                break;
            case PT_INVALID:
            default:
                break;
            }
        }
    }

    String jsonString;
    serializeJson(exportDoc, jsonString);
    return jsonString;
}

bool Storage::importFromJson(const String &jsonString)
{
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error)
    {
        LLOGI("Failed to parse JSON: %s", error.c_str());
        return false;
    }

    preferences.begin(preferencesName, false);
    for (JsonPair kv : doc.as<JsonObject>())
    {
        String key = kv.key().c_str();
        JsonVariant value = kv.value();
        if (value.is<const char *>())
        {
            preferences.putString(compressKey(key).c_str(), value.as<const char *>());
            LLOGI("Imported JSON as string: %s = %s", key.c_str(), value.as<const char *>());
        }
        else if (value.is<int>())
        {
            preferences.putInt(compressKey(key).c_str(), value.as<int>());
            LLOGI("Imported JSON as int: %s = %d", key.c_str(), value.as<int>());
        }
        else if (value.is<float>())
        {
            preferences.putFloat(compressKey(key).c_str(), value.as<float>());
            
            LLOGI("Imported JSON as float: %s = %f", key.c_str(), value.as<float>());
        }
        else if (value.is<bool>())
        {
            preferences.putBool(compressKey(key).c_str(), value.as<bool>());
            LLOGI("Imported JSON as bool: %s = %d", key.c_str(), value.as<bool>());
        }
    }
// LLOGI("Imported JSON: %s", jsonString.c_str());
    return true;
}

// Implement exportToJson and importFromJson methods as before