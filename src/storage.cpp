#include "storage.h"
#include <LittleFS.h>
#include "config.h"
#include "logger.h"
#include "time_manager.h"

Preferences Storage::preferences;
bool Storage::isFsMounted = false;

bool Storage::init() {
    isFsMounted = setupFileSystem();
    LogInitHelper::setFsMounted(isFsMounted);
    
    if (isFsMounted) {
        Logger::log(LOG_SUCCESS, "STORAGE", "LittleFS mounted successfully.");
    } else {
        Logger::log(LOG_ERROR, "STORAGE", "LittleFS mount failed! Internal drive is disabled.");
    }
    
    // Test Preferences access and initialize default settings on first boot
    preferences.begin("esp-settings", false);
    if (!preferences.isKey("initialized")) {
        Logger::log(LOG_INFO, "STORAGE", "First boot detected. Initializing default settings in NVS...");
        preferences.putBool("initialized", true);
        preferences.putBool("darkMode", true);
        preferences.putString("accent", "#00F2FE");
        preferences.putUInt("scrollSpeed", 250);
        preferences.putBool("is24Hour", false);
        preferences.putBool("backlightOn", true);
        preferences.putBool("displayOn", true);
        preferences.putString("hostname", DEFAULT_HOSTNAME);
        preferences.putBool("notifs", true);
        preferences.putString("wifiSsid", DEFAULT_WIFI_SSID);
        preferences.putString("wifiPass", DEFAULT_WIFI_PASSWORD);
        preferences.putBool("useStaticIp", false);
        preferences.putString("staticIp", "");
        preferences.putString("gateway", "");
        preferences.putString("subnet", "");
        preferences.putString("dns", "");
    }
    preferences.end();
    
    return isFsMounted;
}

bool Storage::isReady() {
    return isFsMounted;
}

bool Storage::setupFileSystem() {
    // Try to mount LittleFS
    if (!LittleFS.begin(true)) { // true enables formatting if mount fails
        Serial.println("LittleFS Mount Failed. Formatting storage...");
        if (!LittleFS.format()) {
            Serial.println("LittleFS Format Failed!");
            return false;
        }
        if (!LittleFS.begin(true)) {
            Serial.println("LittleFS Mount Failed even after formatting!");
            return false;
        }
    }
    
    // Ensure system.log and history.json files exist
    if (!LittleFS.exists("/system.log")) {
        File file = LittleFS.open("/system.log", "w");
        if (file) {
            file.println("[SYSTEM] Log file initialized.");
            file.close();
        }
    }
    
    if (!LittleFS.exists("/history.json")) {
        File file = LittleFS.open("/history.json", "w");
        if (file) {
            file.println("[]");
            file.close();
        }
    }
    
    return true;
}

SystemSettings Storage::loadSettings() {
    SystemSettings settings;
    preferences.begin("esp-settings", true); // Read-only mode
    
    settings.darkMode = preferences.getBool("darkMode", true);
    settings.accentColor = preferences.getString("accent", "#00F2FE");
    settings.scrollSpeed = preferences.getUInt("scrollSpeed", 250); // ms delay
    settings.is24Hour = preferences.getBool("is24Hour", false);
    settings.backlightOn = preferences.getBool("backlightOn", true);
    settings.displayOn = preferences.getBool("displayOn", true);
    settings.hostname = preferences.getString("hostname", DEFAULT_HOSTNAME);
    settings.notificationsEnabled = preferences.getBool("notifs", true);
    
    // WiFi configuration
    settings.wifiSsid = preferences.getString("wifiSsid", DEFAULT_WIFI_SSID);
    settings.wifiPassword = preferences.getString("wifiPass", DEFAULT_WIFI_PASSWORD);
    settings.useStaticIp = preferences.getBool("useStaticIp", false);
    settings.staticIp = preferences.getString("staticIp", "");
    settings.gateway = preferences.getString("gateway", "");
    settings.subnet = preferences.getString("subnet", "");
    settings.dns = preferences.getString("dns", "");
    
    preferences.end();
    return settings;
}

void Storage::saveSettings(const SystemSettings& settings) {
    preferences.begin("esp-settings", false); // Read-write
    
    preferences.putBool("darkMode", settings.darkMode);
    preferences.putString("accent", settings.accentColor);
    preferences.putUInt("scrollSpeed", settings.scrollSpeed);
    preferences.putBool("is24Hour", settings.is24Hour);
    preferences.putBool("backlightOn", settings.backlightOn);
    preferences.putBool("displayOn", settings.displayOn);
    preferences.putString("hostname", settings.hostname);
    preferences.putBool("notifs", settings.notificationsEnabled);
    
    preferences.putString("wifiSsid", settings.wifiSsid);
    preferences.putString("wifiPass", settings.wifiPassword);
    preferences.putBool("useStaticIp", settings.useStaticIp);
    preferences.putString("staticIp", settings.staticIp);
    preferences.putString("gateway", settings.gateway);
    preferences.putString("subnet", settings.subnet);
    preferences.putString("dns", settings.dns);
    
    preferences.end();
    Logger::log(LOG_SUCCESS, "STORAGE", "System settings saved permanently.");
}

void Storage::saveSingleSetting(const char* key, const String& value) {
    preferences.begin("esp-settings", false);
    preferences.putString(key, value);
    preferences.end();
    Logger::log(LOG_INFO, "STORAGE", "Saved setting %s = %s", key, value.c_str());
}

void Storage::saveSingleSetting(const char* key, bool value) {
    preferences.begin("esp-settings", false);
    preferences.putBool(key, value);
    preferences.end();
    Logger::log(LOG_INFO, "STORAGE", "Saved setting %s = %s", key, value ? "true" : "false");
}

void Storage::saveSingleSetting(const char* key, int value) {
    preferences.begin("esp-settings", false);
    preferences.putInt(key, value);
    preferences.end();
    Logger::log(LOG_INFO, "STORAGE", "Saved setting %s = %d", key, value);
}

bool Storage::addHistoryItem(const String& message, const String& sender) {
    if (!isFsMounted) return false;
    
    File file = LittleFS.open("/history.json", "r");
    if (!file) {
        Logger::log(LOG_ERROR, "STORAGE", "Failed to open history.json for reading.");
        return false;
    }
    
    // Load and parse JSON
    size_t size = file.size();
    std::unique_ptr<char[]> buf(new char[size + 1]);
    file.readBytes(buf.get(), size);
    buf[size] = '\0';
    file.close();
    
    DynamicJsonDocument doc(16384); // Up to 100 history items
    DeserializationError error = deserializeJson(doc, buf.get());
    
    if (error) {
        // Corrupt file, re-initialize
        doc.clear();
        doc.to<JsonArray>();
    }
    
    JsonArray array = doc.as<JsonArray>();
    
    // Create new item
    JsonObject newItem = array.createNestedObject();
    newItem["text"] = message;
    newItem["time"] = TimeManager::getFormattedDateTime();
    newItem["sender"] = sender;
    
    // Enforce max 100 items
    while (array.size() > HISTORY_MAX_ITEMS) {
        array.remove(0);
    }
    
    // Save back to file
    file = LittleFS.open("/history.json", "w");
    if (!file) {
        Logger::log(LOG_ERROR, "STORAGE", "Failed to open history.json for writing.");
        return false;
    }
    
    if (serializeJson(doc, file) == 0) {
        Logger::log(LOG_ERROR, "STORAGE", "Failed to write history data to file.");
        file.close();
        return false;
    }
    
    file.close();
    return true;
}

String Storage::getHistoryJson() {
    if (!isFsMounted) return "[]";
    
    File file = LittleFS.open("/history.json", "r");
    if (!file) return "[]";
    
    String content = file.readString();
    file.close();
    
    return content.length() > 0 ? content : "[]";
}

bool Storage::clearHistory() {
    if (!isFsMounted) return false;
    
    File file = LittleFS.open("/history.json", "w");
    if (!file) return false;
    
    file.println("[]");
    file.close();
    Logger::log(LOG_INFO, "STORAGE", "LCD message history cleared.");
    return true;
}

bool Storage::importHistory(const String& jsonContent) {
    if (!isFsMounted) return false;
    
    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, jsonContent);
    if (error) {
        Logger::log(LOG_ERROR, "STORAGE", "Failed to parse imported history JSON: %s", error.c_str());
        return false;
    }
    
    if (!doc.is<JsonArray>()) {
        Logger::log(LOG_ERROR, "STORAGE", "Imported history is not a JSON Array.");
        return false;
    }
    
    File file = LittleFS.open("/history.json", "w");
    if (!file) return false;
    
    serializeJson(doc, file);
    file.close();
    
    Logger::log(LOG_SUCCESS, "STORAGE", "History imported successfully. Items: %d", doc.as<JsonArray>().size());
    return true;
}
