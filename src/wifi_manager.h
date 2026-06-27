#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

class WiFiManager {
public:
    static void init();
    static void handle();
    static bool isConnected();
    static String getSSID();
    static int32_t getRSSI();
    static String getIP();
    static String getGateway();
    static String getDNS();
    static String getSubnet();
    static String getHostname();
    static bool testInternet();

private:
    static void connect();
    static void setupMDNS();
    
    static unsigned long lastConnectAttempt;
    static bool isConnecting;
};

#endif // WIFI_MANAGER_H
