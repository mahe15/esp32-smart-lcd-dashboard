#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>

struct SystemSettings {
    bool darkMode;
    String accentColor;
    uint32_t scrollSpeed;
    bool is24Hour;
    bool backlightOn;
    bool displayOn;
    String hostname;
    bool notificationsEnabled;
    String wifiSsid;
    String wifiPassword;
    bool useStaticIp;
    String staticIp;
    String gateway;
    String subnet;
    String dns;
};

class Storage {
public:
    static bool init();
    static bool isReady();
    
    // Settings configuration
    static SystemSettings loadSettings();
    static void saveSettings(const SystemSettings& settings);
    static void saveSingleSetting(const char* key, const String& value);
    static void saveSingleSetting(const char* key, bool value);
    static void saveSingleSetting(const char* key, int value);
    
    // Message history management
    static bool addHistoryItem(const String& message, const String& sender);
    static String getHistoryJson();
    static bool clearHistory();
    static bool importHistory(const String& jsonContent);

private:
    static bool setupFileSystem();
    static Preferences preferences;
    static bool isFsMounted;
};

// Forward declaration of log helper
namespace LogInitHelper {
    void setFsMounted(bool flag);
}

#endif // STORAGE_H
