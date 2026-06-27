#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// WIFI SETTINGS
// ==========================================
#define DEFAULT_WIFI_SSID       "Blueline"
#define DEFAULT_WIFI_PASSWORD   "12345678"
#define DEFAULT_HOSTNAME        "esp32-lcd"
#define WIFI_RECONNECT_INTERVAL 10000 // ms
#define PING_HOST               "8.8.8.8"

// ==========================================
// LCD SETTINGS
// ==========================================
#define LCD_SDA_PIN             21
#define LCD_SCL_PIN             22
#define LCD_COLS                16
#define LCD_ROWS                2
#define DEFAULT_LCD_ADDR        0x27
#define ALTERNATE_LCD_ADDR      0x3F

// ==========================================
// WEB SERVER & SECURITY SETTINGS
// ==========================================
#define HTTP_PORT               80
#define WS_PATH                 "/ws"
#define HTTP_ADMIN_USER         "admin"
#define HTTP_ADMIN_PASS         "admin123" // Change in production!
#define SESSION_TIMEOUT_MS      1800000    // 30 minutes in ms

// ==========================================
// NTP & TIME SETTINGS
// ==========================================
#define NTP_SERVER_1            "pool.ntp.org"
#define NTP_SERVER_2            "time.nist.gov"
#define DEFAULT_TIMEZONE_OFFSET 19800      // +5:30 in seconds (India)
#define NTP_UPDATE_INTERVAL_MS  3600000    // 1 hour in ms

// ==========================================
// SYSTEM SETTINGS
// ==========================================
#define WATCHDOG_TIMEOUT_S      8          // Watchdog timeout in seconds
#define HISTORY_MAX_ITEMS       100
#define TELEMETRY_INTERVAL_MS   1000       // WebSocket broadcast interval (1s)
#define MAX_LOG_LENGTH          2048       // Maximum log buffer size in LittleFS

#endif // CONFIG_H
