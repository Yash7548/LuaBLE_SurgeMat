#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <map>
#include <functional>
#include <type_traits>
#include "config.h"
extern "C"
{
#define LUAT_LOG_TAG "STORAGE_CTRL"
#include "luat_log.h"
}

#define STORAGE Storage::getInstance()

class Storage
{
public:
    static Storage &getInstance()
    {
        static Storage instance("helios");
        return instance;
    }

    void initPreferences();
    void clearPreferences();

    template <typename T>
    void bindVariable(const String &key, T &variable)
    {
        _variables[key] = &variable;

        loadVariable(key, variable);
        // //Serial.println(variable);
        String value = String(variable);
        // INFO_PRINT("Bound variable: %s with value: ", key.c_str());
        // //LLOGI(value.c_str());
    }

    template <typename T>
    void updateVariable(const String &key, const T &value)
    {
        store(key, value);
        if (_variables.count(key) > 0)
        {
            *static_cast<T *>(_variables[key]) = value;
        }
        if (_callbacks.count(key) > 0)
        {
            _callbacks[key]();
        }
    }

    template <typename T>
    T getValue(const String &key, T defaultValue)
    {
        if (_variables.count(key) > 0)
        {
            return *static_cast<T *>(_variables[key]);
        }
        return getValueFromPreferences(key, defaultValue);
    }

    void setCallback(const String &key, std::function<void()> callback)
    {
        _callbacks[key] = callback;
    }

    bool store(const String &key, const String &value);
    bool store(const String &key, int value);
    bool store(const String &key, float value);
    bool store(const String &key, double value);
    bool store(const String &key, bool value);
    String getString(const String &key, const String &defaultValue = "");
    int getInt(const String &key, int defaultValue = 0);
    float getFloat(const String &key, float defaultValue = 0.0f);
    double getDouble(const String &key, double defaultValue = 0.0);
    bool getBool(const String &key, bool defaultValue = false);
    bool remove(const String &key);
    void clear();
    String exportToJson(const String &jsonKeys);
    bool importFromJson(const String &jsonString);

private:
    Storage(const char *preferencesName);
    ~Storage();
    Storage(const Storage &) = delete;
    Storage &operator=(const Storage &) = delete;

    String compressKey(const String &key);
    Preferences preferences;
    const char *preferencesName;
    std::map<String, void *> _variables;
    std::map<String, std::function<void()>> _callbacks;


void loadVariable(const String &key, int &variable)
{
    variable = getValueFromPreferences(key, variable);
    //LLOGI("loadVariable_INT: %s = %d", key.c_str(), variable);
}

void loadVariable(const String &key, float &variable)
{
    variable = getValueFromPreferences(key, variable);
    //LLOGI("loadVariable_FLOAT: %s = %f", key.c_str(), variable);
}

void loadVariable(const String &key, double &variable)
{
    variable = getValueFromPreferences(key, variable);
    //LLOGI("loadVariable_DOUBLE: %s = %f", key.c_str(), variable);
}

void loadVariable(const String &key, bool &variable)
{
    variable = getValueFromPreferences(key, variable);
    //LLOGI("loadVariable_BOOL: %s = %d", key.c_str(), variable);
}

void loadVariable(const String &key, String &variable)
{
    variable = getValueFromPreferences(key, variable);
    //LLOGI("loadVariable_STRING: %s = %s", key.c_str(), variable.c_str());
}


int getValueFromPreferences(const String &key, int defaultValue)
{
    return getInt(key, defaultValue);
}

float getValueFromPreferences(const String &key, float defaultValue)
{
    return getFloat(key, defaultValue);
}

double getValueFromPreferences(const String &key, double defaultValue)
{
    return getDouble(key, defaultValue);
}

bool getValueFromPreferences(const String &key, bool defaultValue)
{
    return getBool(key, defaultValue);
}

String getValueFromPreferences(const String &key, String defaultValue)
{
    return getString(key, defaultValue);
}


//     template <typename T>
//     void loadVariable(const String &key, T &variable)
//     {
//         variable = getValueFromPreferences(key, variable);
//     }

//   template <typename T>
// T getValueFromPreferences(const String &key, T defaultValue)
// {
//     if (std::is_same<T, int>::value)
//     {
//         return static_cast<T>(getInt(key, static_cast<int>(defaultValue)));
//     }
//     else if (std::is_same<T, float>::value)
//     {
//  return static_cast<T>(getFloat(key, static_cast<double>(defaultValue)));
//     }
//     else if (std::is_same<T, double>::value)
//     {
//         return static_cast<T>(getDouble(key, static_cast<double>(defaultValue)));
//     }
//     else if (std::is_same<T, bool>::value)
//     {
//         return static_cast<T>(getBool(key, static_cast<bool>(defaultValue)));
//     }
//     // else if (std::is_same<T, String>::value)
//     // {
//     //    String value = getString(key, String(defaultValue));
//     //    return String(value);
//     // }
//     return defaultValue;
// }



};
#endif