#include "time_manager.h"
#include "storage.h"
#include "logger.h"
#include <time.h>
#include <WiFi.h>

WiFiUDP TimeManager::ntpUDP;
// Initialize NTP client with local config
NTPClient TimeManager::timeClient(TimeManager::ntpUDP, NTP_SERVER_1, DEFAULT_TIMEZONE_OFFSET, NTP_UPDATE_INTERVAL_MS);
bool TimeManager::timeSynced = false;
long TimeManager::timezoneOffset = DEFAULT_TIMEZONE_OFFSET;
bool TimeManager::use24HourFormat = false;
unsigned long TimeManager::lastNtpUpdateCheck = 0;

void TimeManager::init() {
    SystemSettings settings = Storage::loadSettings();
    timezoneOffset = settings.is24Hour ? DEFAULT_TIMEZONE_OFFSET : DEFAULT_TIMEZONE_OFFSET; // Just read from settings
    use24HourFormat = settings.is24Hour;
    
    // Check if user set custom timezone offset in preferences
    Preferences prefs;
    prefs.begin("esp-settings", true);
    timezoneOffset = prefs.getLong("timezoneOffset", DEFAULT_TIMEZONE_OFFSET);
    prefs.end();
    
    timeClient.setTimeOffset(timezoneOffset);
    timeClient.begin();
    
    Logger::log(LOG_INFO, "TIME", "NTP Service initialized. Server: %s, Offset: %d sec", NTP_SERVER_1, timezoneOffset);
}

void TimeManager::handle() {
    unsigned long now = millis();
    
    // Check for updates periodically
    if (now - lastNtpUpdateCheck > 5000 || lastNtpUpdateCheck == 0) {
        lastNtpUpdateCheck = now;
        
        if (WiFi.status() == WL_CONNECTED) {
            bool success = timeClient.update();
            if (success) {
                if (!timeSynced) {
                    timeSynced = true;
                    Logger::log(LOG_SUCCESS, "TIME", "NTP time synchronized. Current time: %s", getFormattedDateTime().c_str());
                }
            } else if (!timeSynced) {
                Logger::log(LOG_WARNING, "TIME", "Waiting for NTP sync response...");
            }
        }
    }
}

bool TimeManager::isSynced() {
    return timeSynced;
}

String TimeManager::getFormattedTimeOnly() {
    if (!timeSynced) {
        return "00:00:00";
    }
    
    unsigned long epochTime = timeClient.getEpochTime();
    struct tm *timeInfo = gmtime((time_t *)&epochTime);
    
    char buffer[12];
    if (use24HourFormat) {
        strftime(buffer, sizeof(buffer), "%H:%M:%S", timeInfo);
    } else {
        int hour = timeInfo->tm_hour;
        const char* ampm = (hour >= 12) ? "PM" : "AM";
        hour = hour % 12;
        if (hour == 0) hour = 12;
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d %s", hour, timeInfo->tm_min, timeInfo->tm_sec, ampm);
    }
    
    return String(buffer);
}

String TimeManager::getFormattedDateOnly() {
    if (!timeSynced) {
        return "1970-01-01";
    }
    
    unsigned long epochTime = timeClient.getEpochTime();
    struct tm *timeInfo = gmtime((time_t *)&epochTime);
    
    char buffer[12];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", timeInfo);
    return String(buffer);
}

String TimeManager::getFormattedDateTime() {
    return getFormattedDateOnly() + " " + getFormattedTimeOnly();
}

void TimeManager::updateTimezone(long offsetSeconds) {
    timezoneOffset = offsetSeconds;
    timeClient.setTimeOffset(timezoneOffset);
    
    Preferences prefs;
    prefs.begin("esp-settings", false);
    prefs.putLong("timezoneOffset", offsetSeconds);
    prefs.end();
    
    Logger::log(LOG_INFO, "TIME", "Timezone updated. Offset: %ld seconds. Current: %s", offsetSeconds, getFormattedTimeOnly().c_str());
}

void TimeManager::updateHourFormat(bool is24Hour) {
    use24HourFormat = is24Hour;
    Logger::log(LOG_INFO, "TIME", "Hour format updated to %s", is24Hour ? "24-hour" : "12-hour");
}
