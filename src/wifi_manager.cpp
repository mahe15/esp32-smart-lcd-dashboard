#include "wifi_manager.h"
#include <ESPmDNS.h>
#include <ESP32Ping.h>
#include "storage.h"
#include "logger.h"

unsigned long WiFiManager::lastConnectAttempt = 0;
bool WiFiManager::isConnecting = false;

void WiFiManager::init() {
    connect();
}

void WiFiManager::connect() {
    SystemSettings settings = Storage::loadSettings();
    
    // Set WiFi hostname
    WiFi.setHostname(settings.hostname.c_str());
    
    // Check if credentials are still set to default placeholder
    if (settings.wifiSsid == "YOUR_SSID" || settings.wifiSsid.length() == 0) {
        Logger::log(LOG_WARNING, "WIFI", "Default credentials detected. Starting Access Point fallback...");
        WiFi.mode(WIFI_AP);
        String apName = "ESP32-Smart-LCD-" + String((uint32_t)ESP.getEfuseMac(), HEX);
        WiFi.softAP(apName.c_str(), "admin123");
        Logger::log(LOG_SUCCESS, "WIFI", "AP Hotspot Active: SSID: %s | Pwd: %s", apName.c_str(), "admin123");
        Logger::log(LOG_INFO, "WIFI", "Access Dashboard at IP: %s", WiFi.softAPIP().toString().c_str());
        setupMDNS();
        return;
    }
    
    Logger::log(LOG_INFO, "WIFI", "Connecting to SSID: %s...", settings.wifiSsid.c_str());
    
    WiFi.mode(WIFI_AP_STA); // AP + Station mode for fallback configuration capability
    String apName = "ESP32-Smart-LCD-Setup";
    WiFi.softAP(apName.c_str(), "admin123");
    
    if (settings.useStaticIp) {
        IPAddress ip, gateway, subnet, dns;
        if (ip.fromString(settings.staticIp) && 
            gateway.fromString(settings.gateway) && 
            subnet.fromString(settings.subnet)) {
            
            if (settings.dns.length() > 0) {
                dns.fromString(settings.dns);
                WiFi.config(ip, gateway, subnet, dns);
            } else {
                WiFi.config(ip, gateway, subnet);
            }
            Logger::log(LOG_INFO, "WIFI", "Using Static IP Configuration.");
        } else {
            Logger::log(LOG_ERROR, "WIFI", "Invalid Static IP credentials in settings. Using DHCP instead.");
        }
    }
    
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
    lastConnectAttempt = millis();
    isConnecting = true;
}

void WiFiManager::handle() {
    wl_status_t status = WiFi.status();
    
    if (isConnecting) {
        if (status == WL_CONNECTED) {
            isConnecting = false;
            WiFi.mode(WIFI_STA); // Turn off AP if successfully connected to router
            Logger::log(LOG_SUCCESS, "WIFI", "Connected to WiFi! IP: %s", WiFi.localIP().toString().c_str());
            setupMDNS();
            Logger::printSystemBanner();
        } else if (millis() - lastConnectAttempt > 15000) { // Timeout after 15 seconds
            Logger::log(LOG_WARNING, "WIFI", "WiFi connection timeout. Staying in AP+STA setup mode.");
            isConnecting = false;
        }
    } else {
        // Handle unexpected disconnection
        if (status != WL_CONNECTED && (WiFi.getMode() & WIFI_MODE_STA)) {
            unsigned long now = millis();
            if (now - lastConnectAttempt > WIFI_RECONNECT_INTERVAL) {
                Logger::log(LOG_WARNING, "WIFI", "WiFi disconnected. Reconnecting...");
                WiFi.reconnect();
                lastConnectAttempt = now;
            }
        }
    }
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getSSID() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.SSID();
    }
    return (WiFi.getMode() == WIFI_MODE_AP) ? WiFi.softAPSSID() : "Disconnected";
}

int32_t WiFiManager::getRSSI() {
    return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -100;
}

String WiFiManager::getIP() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return WiFi.softAPIP().toString();
}

String WiFiManager::getGateway() {
    return WiFi.status() == WL_CONNECTED ? WiFi.gatewayIP().toString() : "0.0.0.0";
}

String WiFiManager::getDNS() {
    return WiFi.status() == WL_CONNECTED ? WiFi.dnsIP().toString() : "0.0.0.0";
}

String WiFiManager::getSubnet() {
    return WiFi.status() == WL_CONNECTED ? WiFi.subnetMask().toString() : "0.0.0.0";
}

String WiFiManager::getHostname() {
    SystemSettings settings = Storage::loadSettings();
    return settings.hostname;
}

bool WiFiManager::testInternet() {
    if (!isConnected()) return false;
    // Ping Google DNS (8.8.8.8) to check internet access
    return Ping.ping(PING_HOST, 1);
}

void WiFiManager::setupMDNS() {
    SystemSettings settings = Storage::loadSettings();
    if (MDNS.begin(settings.hostname.c_str())) {
        Logger::log(LOG_SUCCESS, "MDNS", "mDNS responder started. Hostname: http://%s.local", settings.hostname.c_str());
        MDNS.addService("http", "tcp", HTTP_PORT);
    } else {
        Logger::log(LOG_ERROR, "MDNS", "Error starting mDNS responder.");
    }
}
