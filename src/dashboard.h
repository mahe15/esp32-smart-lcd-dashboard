#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <Arduino.h>
#include <ArduinoJson.h>

class Dashboard {
public:
    static void init();
    static void incrementBootCount();
    
    // Telemetry
    static uint32_t getBootCount();
    static uint32_t getUptimeSeconds();
    static String getTelemetryJson(int connectedClients);
    static void getSystemMetrics(size_t &heapFree, uint32_t &uptime, int &rssi, size_t &sketchFree);
    
private:
    static uint32_t bootCount;
    static unsigned long bootTimeMs;
};

#endif // DASHBOARD_H
