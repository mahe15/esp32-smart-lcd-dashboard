#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

class LCDManager {
public:
    static void init();
    static void handle();
    
    // Status
    static bool isLcdConnected();
    static uint8_t getLcdAddress();
    static String getCurrentText();
    static bool getBacklightState();
    static bool getDisplayState();
    
    // LCD Actions
    static void printText(const String& text, bool center = false, bool scroll = false);
    static void clear();
    static void setBacklight(bool on);
    static void setDisplay(bool on);
    static void setCursorBlink(bool blink);
    static void setMode(int mode);
    static int getMode();
    
    // Quick Actions
    static void showSystemInfo(int mode); // 1: Time, 2: IP, 3: RSSI
    static void playWelcomeAnimation();
    
    // Emoji mapping
    static String parseEmojis(const String& input);

private:
    static uint8_t scanI2CBus();
    static void initCustomCharacters();
    static void updateScrolling();
    
    static LiquidCrystal_I2C* lcd;
    static uint8_t lcdAddress;
    static bool lcdFound;
    static bool backlightState;
    static bool displayState;
    static bool cursorBlinkState;
    
    static String currentRawText;
    static String parsedText;
    static bool shouldScroll;
    static bool centerText;
    
    static int lcdMode;
    static void updateLiveMode();
    
    // Software scrolling state
    static unsigned long lastScrollTime;
    static int scrollIndexRow0;
    static int scrollIndexRow1;
    static String line0;
    static String line1;
};

#endif // LCD_MANAGER_H
