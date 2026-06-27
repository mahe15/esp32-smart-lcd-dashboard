#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "config.h"

enum LogLevel {
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_SUCCESS
};

class Logger {
public:
    static void init();
    static void log(LogLevel level, const char* sender, const char* format, ...);
    static void logRequest(const char* clientIp, const char* endpoint, uint32_t execTimeMs, size_t heapFree, int statusCode);
    static void printSystemBanner();
    
    // Log history management
    static String getLogsJson();
    static void clearLogs();
    static void writeLogToFile(const String& logMsg);

private:
    static String levelToString(LogLevel level);
    static String getFormattedTime();
};

#endif // LOGGER_H
