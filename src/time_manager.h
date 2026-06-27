#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

class TimeManager {
public:
    static void init();
    static void handle();
    static bool isSynced();
    static String getFormattedTimeOnly();
    static String getFormattedDateOnly();
    static String getFormattedDateTime();
    static void updateTimezone(long offsetSeconds);
    static void updateHourFormat(bool is24Hour);

private:
    static WiFiUDP ntpUDP;
    static NTPClient timeClient;
    static bool timeSynced;
    static long timezoneOffset;
    static bool use24HourFormat;
    static unsigned long lastNtpUpdateCheck;
};

#endif // TIME_MANAGER_H
