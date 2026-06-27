#include "logger.h"
#include <FS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include "time_manager.h"

// Static memory buffer for Web logs
static const int MAX_MEM_LOGS = 40;
static String memLogs[MAX_MEM_LOGS];
static int memLogHead = 0;
static int memLogCount = 0;
static bool fsReady = false;

void Logger::init() {
    Serial.begin(115200);
    while (!Serial && millis() < 1000); // Wait for serial initialization
    log(LOG_INFO, "SYSTEM", "Serial Logging initialized at 115200 baud.");
}

void Logger::log(LogLevel level, const char* sender, const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    String timeStr = getFormattedTime();
    String levelStr = levelToString(level);
    
    // Colored console output using ANSI codes
    String ansiColor = "\033[0m"; // Reset
    switch (level) {
        case LOG_INFO:    ansiColor = "\033[36m"; break; // Cyan
        case LOG_WARNING: ansiColor = "\033[33m"; break; // Yellow
        case LOG_ERROR:   ansiColor = "\033[31m"; break; // Red
        case LOG_SUCCESS: ansiColor = "\033[32m"; break; // Green
    }
    
    // Print to Serial
    Serial.printf("%s[%s][%s][%s] %s\033[0m\n", ansiColor.c_str(), timeStr.c_str(), levelStr.c_str(), sender, buffer);
    
    // Create combined plain string for web/file
    String logLine = "[" + timeStr + "][" + levelStr + "][" + String(sender) + "] " + String(buffer);
    
    // Write to memory buffer
    memLogs[memLogHead] = logLine;
    memLogHead = (memLogHead + 1) % MAX_MEM_LOGS;
    if (memLogCount < MAX_MEM_LOGS) {
        memLogCount++;
    }
    
    // Write to LittleFS file if available
    if (fsReady) {
        writeLogToFile(logLine);
    }
}

void Logger::logRequest(const char* clientIp, const char* endpoint, uint32_t execTimeMs, size_t heapFree, int statusCode) {
    LogLevel level = LOG_INFO;
    if (statusCode >= 400 && statusCode < 500) {
        level = LOG_WARNING;
    } else if (statusCode >= 500 || statusCode < 0) {
        level = LOG_ERROR;
    } else if (statusCode >= 200 && statusCode < 300) {
        level = LOG_SUCCESS;
    }
    
    log(level, "HTTP", "Client: %s | URL: %s | Code: %d | Time: %d ms | Free Heap: %d B", 
        clientIp, endpoint, statusCode, execTimeMs, heapFree);
}

void Logger::printSystemBanner() {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    uint32_t flash_size = spi_flash_get_chip_size();
    
    Serial.println("\n========================================================");
    Serial.println("                ESP32 SMART LCD SERVER                  ");
    Serial.println("========================================================");
    Serial.printf(" Chip Model        : %s\n", ESP.getChipModel());
    Serial.printf(" Chip Revision     : %d\n", ESP.getChipRevision());
    Serial.printf(" CPU Frequency     : %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf(" Flash Size        : %d MB\n", flash_size / (1024 * 1024));
    Serial.printf(" Sketch Size       : %d KB (Free: %d KB)\n", ESP.getSketchSize() / 1024, ESP.getFreeSketchSpace() / 1024);
    Serial.printf(" Free Heap         : %d KB\n", ESP.getFreeHeap() / 1024);
    Serial.printf(" SDK Version       : %s\n", ESP.getSdkVersion());
    Serial.printf(" MAC Address       : %s\n", WiFi.macAddress().c_str());
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf(" WiFi SSID         : %s (RSSI: %d dBm)\n", WiFi.SSID().c_str(), WiFi.RSSI());
        Serial.printf(" IP Address        : %s\n", WiFi.localIP().toString().c_str());
        Serial.printf(" Gateway IP        : %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf(" DNS IP            : %s\n", WiFi.dnsIP().toString().c_str());
        Serial.printf(" Subnet Mask       : %s\n", WiFi.subnetMask().toString().c_str());
    } else {
        Serial.println(" WiFi SSID         : Disconnected");
        Serial.println(" IP Address        : N/A");
    }
    
    Serial.printf(" LittleFS Status   : %s\n", fsReady ? "Mounted successfully" : "Failed / Not initialized");
    Serial.println("========================================================\n");
}

void Logger::writeLogToFile(const String& logMsg) {
    if (!fsReady) return;
    
    File logFile = LittleFS.open("/system.log", "a");
    if (!logFile) {
        Serial.println("Failed to open log file for writing.");
        return;
    }
    
    logFile.println(logMsg);
    
    // Check if file size exceeds MAX_LOG_LENGTH to prevent taking up all flash
    if (logFile.size() > MAX_LOG_LENGTH) {
        logFile.close();
        // Rotate: clear and rewrite current buffer
        logFile = LittleFS.open("/system.log", "w");
        if (logFile) {
            logFile.println("[SYSTEM] Log rotated due to size limit.");
            int startIdx = (memLogHead - memLogCount + MAX_MEM_LOGS) % MAX_MEM_LOGS;
            for (int i = 0; i < memLogCount; i++) {
                int idx = (startIdx + i) % MAX_MEM_LOGS;
                logFile.println(memLogs[idx]);
            }
        }
    }
    logFile.close();
}

String Logger::getLogsJson() {
    String json = "[";
    int startIdx = (memLogHead - memLogCount + MAX_MEM_LOGS) % MAX_MEM_LOGS;
    for (int i = 0; i < memLogCount; i++) {
        int idx = (startIdx + i) % MAX_MEM_LOGS;
        // Escape quotes and backslashes in the logs for json validation
        String escapedLine = memLogs[idx];
        escapedLine.replace("\\", "\\\\");
        escapedLine.replace("\"", "\\\"");
        
        json += "\"" + escapedLine + "\"";
        if (i < memLogCount - 1) {
            json += ",";
        }
    }
    json += "]";
    return json;
}

void Logger::clearLogs() {
    memLogHead = 0;
    memLogCount = 0;
    if (fsReady) {
        LittleFS.remove("/system.log");
        File logFile = LittleFS.open("/system.log", "w");
        if (logFile) {
            logFile.println("[SYSTEM] System log cleared.");
            logFile.close();
        }
    }
}

String Logger::levelToString(LogLevel level) {
    switch (level) {
        case LOG_INFO:    return "INFO";
        case LOG_WARNING: return "WARN";
        case LOG_ERROR:   return "ERROR";
        case LOG_SUCCESS: return "OK  ";
        default:          return "UNKN";
    }
}

String Logger::getFormattedTime() {
    return TimeManager::getFormattedTimeOnly();
}

// Add a helper method to configure the LittleFS state flag from storage initialization
namespace LogInitHelper {
    void setFsMounted(bool flag) {
        fsReady = flag;
    }
}
