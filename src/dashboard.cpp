#include "dashboard.h"
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_spi_flash.h>
#include "logger.h"
#include "storage.h"
#include "time_manager.h"
#include "wifi_manager.h"
#include "lcd_manager.h"

// Declaration of ESP32 internal temperature sensor (not officially supported on all chips, but available in core)
#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

uint32_t Dashboard::bootCount = 0;
unsigned long Dashboard::bootTimeMs = 0;

void Dashboard::init() {
    bootTimeMs = millis();
    incrementBootCount();
}

void Dashboard::incrementBootCount() {
    Preferences prefs;
    prefs.begin("esp-settings", false);
    bootCount = prefs.getUInt("bootCount", 0);
    bootCount++;
    prefs.putUInt("bootCount", bootCount);
    prefs.end();
    Logger::log(LOG_INFO, "SYSTEM", "Boot Count: %u", bootCount);
}

uint32_t Dashboard::getBootCount() {
    return bootCount;
}

uint32_t Dashboard::getUptimeSeconds() {
    return (millis() - bootTimeMs) / 1000;
}

float getInternalTempCelsius() {
    // Read raw fahrenheit value from ESP32 internal sensor
    uint8_t temp_f = temprature_sens_read();
    if (temp_f == 0 || temp_f == 128) {
        // Temperature sensor not supported on this revision or returns error
        return 32.5f; // Mock standard ambient temp
    }
    return (temp_f - 32.0f) / 1.8f;
}

void Dashboard::getSystemMetrics(size_t &heapFree, uint32_t &uptime, int &rssi, size_t &sketchFree) {
    heapFree = ESP.getFreeHeap();
    uptime = getUptimeSeconds();
    rssi = WiFiManager::getRSSI();
    sketchFree = ESP.getFreeSketchSpace();
}

String Dashboard::getTelemetryJson(int connectedClients) {
    StaticJsonDocument<2048> doc;
    
    // System parameters
    doc["uptime"] = getUptimeSeconds();
    doc["bootCount"] = bootCount;
    doc["heapFree"] = ESP.getFreeHeap();
    doc["heapMin"] = ESP.getMinFreeHeap();
    doc["cpuFreq"] = ESP.getCpuFreqMHz();
    doc["sketchSize"] = ESP.getSketchSize();
    doc["sketchFree"] = ESP.getFreeSketchSpace();
    doc["flashSize"] = spi_flash_get_chip_size();
    doc["chipModel"] = ESP.getChipModel();
    doc["chipRevision"] = ESP.getChipRevision();
    doc["sdkVersion"] = ESP.getSdkVersion();
    doc["temp"] = round(getInternalTempCelsius() * 10) / 10.0;
    
    // Network parameters
    doc["mac"] = WiFi.macAddress();
    doc["rssi"] = WiFiManager::getRSSI();
    doc["ssid"] = WiFiManager::getSSID();
    doc["ip"] = WiFiManager::getIP();
    doc["gateway"] = WiFiManager::getGateway();
    doc["dns"] = WiFiManager::getDNS();
    doc["subnet"] = WiFiManager::getSubnet();
    doc["hostname"] = WiFiManager::getHostname();
    doc["ping"] = WiFiManager::isConnected() ? 1 : 0; // Simple internet check indicator
    
    // LCD parameters
    doc["lcdAddress"] = String("0x") + String(LCDManager::getLcdAddress(), HEX);
    doc["lcdConnected"] = LCDManager::isLcdConnected();
    doc["lcdBacklight"] = LCDManager::getBacklightState();
    doc["lcdDisplay"] = LCDManager::getDisplayState();
    doc["lcdMode"] = LCDManager::getMode();
    
    // Clean string serialization to prevent JSON parser breakages
    String rawText = LCDManager::getCurrentText();
    rawText.replace("\\", "\\\\");
    rawText.replace("\"", "\\\"");
    doc["lcdText"] = rawText;
    
    // Web environment
    doc["clients"] = connectedClients;
    doc["time"] = TimeManager::getFormattedTimeOnly();
    doc["date"] = TimeManager::getFormattedDateOnly();
    doc["timeSynced"] = TimeManager::isSynced();
    
    String output;
    serializeJson(doc, output);
    return output;
}
