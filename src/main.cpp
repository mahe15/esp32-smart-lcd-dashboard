/**
 * @file main.cpp
 * @brief Production-Grade ESP32 Smart LCD Web Dashboard Entrypoint
 * @author Antigravity AI
 * @date 2026-06-27
 */

#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "logger.h"
#include "storage.h"
#include "wifi_manager.h"
#include "time_manager.h"
#include "lcd_manager.h"
#include "dashboard.h"
#include "web_server.h"

/**
 * @brief Setup system parameters, peripherals, and server resources
 */
void setup() {
    // 1. Initialize Serial Console Logger
    Logger::init();
    Logger::log(LOG_INFO, "BOOT", "Initializing ESP32 Smart LCD Server...");
    
    // 2. Initialize Preferences & Filesystem
    Storage::init();
    
    // 3. Initialize Telemetry and boot statistics
    Dashboard::init();
    
    // 4. Initialize I2C Peripherals and LiquidCrystal LCD
    LCDManager::init();
    
    // 5. Connect WiFi or spin Access Point Fallback
    WiFiManager::init();
    
    // 6. Setup NTP network synchronization client
    TimeManager::init();
    
    // 7. Setup REST endpoints, WS channels and ElegantOTA
    WebServerManager::init();
    
    // 8. Initialize System Watchdog Timer for task reliability
    Logger::log(LOG_INFO, "SYSTEM", "Configuring Watchdog Timer with %d seconds timeout.", WATCHDOG_TIMEOUT_S);
    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true); // Enable panic on timeout
    esp_task_wdt_add(NULL);                      // Add main thread to watchdog monitoring
    
    Logger::log(LOG_SUCCESS, "BOOT", "Setup complete. Entering execution loop.");
}

/**
 * @brief Non-blocking system execution loop
 */
void loop() {
    // Reset/Feed the hardware watchdog timer
    esp_task_wdt_reset();
    
    // Process WiFi connection state and recovery loops
    WiFiManager::handle();
    
    // Process NTP network synchronization refresh rates
    TimeManager::handle();
    
    // Process LCD screen animations and row scroll timers
    LCDManager::handle();
    
    // Process WebSocket client cleanups and broadcast tickers
    WebServerManager::handle();
    
    // Yield to background tasks (FreeRTOS scheduler)
    delay(1);
}
