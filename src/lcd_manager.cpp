#include "lcd_manager.h"
#include <Wire.h>
#include "logger.h"
#include "storage.h"
#include "time_manager.h"
#include "wifi_manager.h"

// Define static members
LiquidCrystal_I2C* LCDManager::lcd = nullptr;
uint8_t LCDManager::lcdAddress = 0x27;
bool LCDManager::lcdFound = false;
bool LCDManager::backlightState = true;
bool LCDManager::displayState = true;
bool LCDManager::cursorBlinkState = false;

String LCDManager::currentRawText = "";
String LCDManager::parsedText = "";
bool LCDManager::shouldScroll = false;
bool LCDManager::centerText = false;
int LCDManager::lcdMode = 0;

unsigned long LCDManager::lastScrollTime = 0;
int LCDManager::scrollIndexRow0 = 0;
int LCDManager::scrollIndexRow1 = 0;
String LCDManager::line0 = "";
String LCDManager::line1 = "";

// Custom characters bitmap definitions (5x8 pixels)
static uint8_t charHeart[8]     = { 0x00, 0x0A, 0x1F, 0x1F, 0x0E, 0x04, 0x00, 0x00 };
static uint8_t charThumbsUp[8]  = { 0x04, 0x0E, 0x04, 0x04, 0x1F, 0x1F, 0x0E, 0x00 };
static uint8_t charSmile[8]     = { 0x00, 0x0A, 0x00, 0x00, 0x11, 0x0E, 0x00, 0x00 };
static uint8_t charFire[8]      = { 0x04, 0x0A, 0x0A, 0x15, 0x15, 0x1F, 0x0E, 0x00 };
static uint8_t charStar[8]      = { 0x04, 0x04, 0x0E, 0x1F, 0x0E, 0x0A, 0x11, 0x00 };

void LCDManager::init() {
    // Initialize I2C bus
    Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
    
    // Scan bus to find LCD address
    lcdAddress = scanI2CBus();
    
    if (lcdAddress == 0) {
        Logger::log(LOG_ERROR, "LCD", "LCD Display was not detected on I2C bus! Check SDA (GPIO21) and SCL (GPIO22).");
        lcdFound = false;
        return;
    }
    
    lcdFound = true;
    Logger::log(LOG_SUCCESS, "LCD", "LCD Display detected at address 0x%02X.", lcdAddress);
    
    // Initialize LCD
    lcd = new LiquidCrystal_I2C(lcdAddress, LCD_COLS, LCD_ROWS);
    lcd->init();
    
    // Read backlight state from storage
    SystemSettings settings = Storage::loadSettings();
    backlightState = settings.backlightOn;
    displayState = settings.displayOn;
    shouldScroll = settings.scrollSpeed > 0;
    
    if (backlightState) {
        lcd->backlight();
    } else {
        lcd->noBacklight();
    }
    
    if (displayState) {
        lcd->display();
    } else {
        lcd->noDisplay();
    }
    
    initCustomCharacters();
    playWelcomeAnimation();
}

uint8_t LCDManager::scanI2CBus() {
    uint8_t addressesToTry[] = { DEFAULT_LCD_ADDR, ALTERNATE_LCD_ADDR };
    
    // First, try the default addresses
    for (uint8_t addr : addressesToTry) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            return addr;
        }
    }
    
    // If not found, scan all possible I2C addresses (0x08 to 0x77)
    Logger::log(LOG_WARNING, "LCD", "LCD not found at default addresses. Performing full I2C scan...");
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            return addr;
        }
    }
    
    return 0; // Not found
}

void LCDManager::initCustomCharacters() {
    if (!lcdFound) return;
    
    // HD44780 has 8 custom character slots (0-7).
    // Note: custom char 0 should be referenced in string as '\x08' because '\x00' terminates C++ string.
    lcd->createChar(0, charHeart);
    lcd->createChar(1, charThumbsUp);
    lcd->createChar(2, charSmile);
    lcd->createChar(3, charFire);
    lcd->createChar(4, charStar);
}

void LCDManager::playWelcomeAnimation() {
    if (!lcdFound) return;
    
    lcd->clear();
    
    // Frame 1: loading effect
    lcd->setCursor(0, 0);
    lcd->print("   Booting up   ");
    lcd->setCursor(0, 1);
    for (int i = 0; i < 16; i++) {
        lcd->print(".");
        delay(40);
    }
    
    delay(100);
    lcd->clear();
    
    // Frame 2: text banner
    lcd->setCursor(0, 0);
    lcd->print("ESP32 SMART LCD");
    lcd->setCursor(0, 1);
    lcd->print("Ready \x02 \x08 \x03 \x04"); // Emojis: Smile, Heart, Fire, Star
    
    delay(1500);
    lcd->clear();
    
    // Initial LCD status
    lcd->setCursor(0, 0);
    lcd->print("IP Address:");
    lcd->setCursor(0, 1);
    lcd->print("AP/DHCP Connecting");
}

void LCDManager::handle() {
    if (!lcdFound) return;
    
    if (lcdMode == 0) {
        // Update scrolling if enabled
        if (shouldScroll) {
            updateScrolling();
        }
    } else {
        // Live mode updates every 1 second
        static unsigned long lastModeUpdate = 0;
        unsigned long now = millis();
        if (now - lastModeUpdate >= 1000) {
            lastModeUpdate = now;
            updateLiveMode();
        }
    }
}

bool LCDManager::isLcdConnected() {
    return lcdFound;
}

uint8_t LCDManager::getLcdAddress() {
    return lcdAddress;
}

String LCDManager::getCurrentText() {
    if (lcdFound) {
        if (lcdMode == 1) {
            return "Time: " + TimeManager::getFormattedTimeOnly() + "\nDate: " + TimeManager::getFormattedDateOnly();
        } else if (lcdMode == 2) {
            return "HN: " + WiFiManager::getHostname().substring(0, 12) + "\nIP: " + WiFiManager::getIP();
        } else if (lcdMode == 3) {
            return "SSID: " + WiFiManager::getSSID().substring(0, 10) + "\nSig: " + String(WiFiManager::getRSSI()) + " dBm";
        }
    }
    return currentRawText;
}

bool LCDManager::getBacklightState() {
    return backlightState;
}

bool LCDManager::getDisplayState() {
    return displayState;
}

void LCDManager::clear() {
    if (!lcdFound) return;
    lcdMode = 0; // Reset to custom text mode
    lcd->clear();
    currentRawText = "";
    line0 = "";
    line1 = "";
    parsedText = "";
}

void LCDManager::setBacklight(bool on) {
    if (!lcdFound) return;
    backlightState = on;
    if (on) {
        lcd->backlight();
    } else {
        lcd->noBacklight();
    }
    Storage::saveSingleSetting("backlightOn", on);
}

void LCDManager::setDisplay(bool on) {
    if (!lcdFound) return;
    displayState = on;
    if (on) {
        lcd->display();
    } else {
        lcd->noDisplay();
    }
    Storage::saveSingleSetting("displayOn", on);
}

void LCDManager::setCursorBlink(bool blink) {
    if (!lcdFound) return;
    cursorBlinkState = blink;
    if (blink) {
        lcd->blink();
    } else {
        lcd->noBlink();
    }
}

String LCDManager::parseEmojis(const String& input) {
    String output = input;
    
    // Heart Emoji
    output.replace("❤️", "\x08");
    output.replace("💖", "\x08");
    output.replace("💕", "\x08");
    output.replace("🖤", "\x08");
    output.replace("💜", "\x08");
    
    // Thumbs Up Emoji
    output.replace("👍", "\x01");
    output.replace("👌", "\x01");
    output.replace("👏", "\x01");
    
    // Smile Emoji
    output.replace("😊", "\x02");
    output.replace("😀", "\x02");
    output.replace("😃", "\x02");
    output.replace("😄", "\x02");
    output.replace("🙂", "\x02");
    
    // Fire Emoji
    output.replace("🔥", "\x03");
    output.replace("⚡", "\x03");
    output.replace("💥", "\x03");
    
    // Star Emoji
    output.replace("⭐", "\x04");
    output.replace("🌟", "\x04");
    output.replace("✨", "\x04");
    
    return output;
}

void LCDManager::printText(const String& text, bool center, bool scroll) {
    if (!lcdFound) return;
    
    lcdMode = 0; // Reset to custom text mode
    currentRawText = text;
    centerText = center;
    shouldScroll = scroll;
    parsedText = parseEmojis(text);
    
    // Reset scrolling counters
    scrollIndexRow0 = 0;
    scrollIndexRow1 = 0;
    lastScrollTime = millis();
    
    // Split into row 0 and row 1
    int newlineIndex = parsedText.indexOf('\n');
    if (newlineIndex != -1) {
        line0 = parsedText.substring(0, newlineIndex);
        line1 = parsedText.substring(newlineIndex + 1);
    } else {
        if (parsedText.length() <= 16) {
            line0 = parsedText;
            line1 = "";
        } else {
            // Cut or prepare for scrolling
            if (scroll) {
                line0 = parsedText;
                line1 = "";
            } else {
                line0 = parsedText.substring(0, 16);
                line1 = parsedText.substring(16, 32);
            }
        }
    }
    
    // Initial display
    lcd->clear();
    
    // Row 0
    lcd->setCursor(0, 0);
    if (centerText && line0.length() < 16) {
        int padding = (16 - line0.length()) / 2;
        for (int i = 0; i < padding; i++) lcd->print(" ");
        lcd->print(line0);
    } else {
        lcd->print(line0.substring(0, 16));
    }
    
    // Row 1
    lcd->setCursor(0, 1);
    if (centerText && line1.length() < 16) {
        int padding = (16 - line1.length()) / 2;
        for (int i = 0; i < padding; i++) lcd->print(" ");
        lcd->print(line1);
    } else {
        lcd->print(line1.substring(0, 16));
    }
}

void LCDManager::updateScrolling() {
    SystemSettings settings = Storage::loadSettings();
    uint32_t delayMs = settings.scrollSpeed;
    if (delayMs == 0) return;
    
    unsigned long now = millis();
    if (now - lastScrollTime > delayMs) {
        lastScrollTime = now;
        bool updated = false;
        
        // Scroll row 0 if it is wider than 16 chars
        if (line0.length() > 16) {
            lcd->setCursor(0, 0);
            String paddedLine0 = line0 + "   " + line0.substring(0, 16);
            lcd->print(paddedLine0.substring(scrollIndexRow0, scrollIndexRow0 + 16));
            scrollIndexRow0 = (scrollIndexRow0 + 1) % (line0.length() + 3);
            updated = true;
        }
        
        // Scroll row 1 if it is wider than 16 chars
        if (line1.length() > 16) {
            lcd->setCursor(0, 1);
            String paddedLine1 = line1 + "   " + line1.substring(0, 16);
            lcd->print(paddedLine1.substring(scrollIndexRow1, scrollIndexRow1 + 16));
            scrollIndexRow1 = (scrollIndexRow1 + 1) % (line1.length() + 3);
            updated = true;
        }
    }
}

void LCDManager::showSystemInfo(int mode) {
    setMode(mode);
}

void LCDManager::setMode(int mode) {
    if (!lcdFound) return;
    lcdMode = mode;
    if (mode > 0) {
        lcd->clear();
        updateLiveMode();
    }
}

int LCDManager::getMode() {
    return lcdMode;
}

void LCDManager::updateLiveMode() {
    if (!lcdFound) return;
    if (lcdMode == 1) { // Time Mode
        lcd->setCursor(0, 0);
        lcd->print("Time: " + TimeManager::getFormattedTimeOnly() + "   ");
        lcd->setCursor(0, 1);
        lcd->print("Date: " + TimeManager::getFormattedDateOnly() + "   ");
    } else if (lcdMode == 2) { // IP Mode
        lcd->setCursor(0, 0);
        lcd->print("HN: " + WiFiManager::getHostname().substring(0, 12) + "    ");
        lcd->setCursor(0, 1);
        lcd->print("IP: " + WiFiManager::getIP() + "      ");
    } else if (lcdMode == 3) { // RSSI Mode
        lcd->setCursor(0, 0);
        lcd->print("SSID: " + WiFiManager::getSSID().substring(0, 10) + "      ");
        lcd->setCursor(0, 1);
        lcd->print("Sig: " + String(WiFiManager::getRSSI()) + " dBm   ");
    }
}
