# ESP32 Smart LCD Web Dashboard

A production-grade, highly-optimized IoT web application for the **ESP32-WROOM-32D** microcontroller to interface with a standard **16x2 HD44780 LCD display** via I2C backpack. It features a responsive SaaS-like Glassmorphism bento dashboard, WebSocket communication, Web Speech API integration, circular storage rotation, and local configuration persistence.

---

## 🔌 Hardware Configuration & Pinout

### Microcontroller
* **ESP32-WROOM-32D** (Dual-core Tensilica Xtensa 32-bit LX6, 240MHz, 4MB Flash, 520KB SRAM)

### Display
* **16x2 LCD Display (HD44780)** equipped with an **I2C backpack (PCF8574)**
* **VCC:** Connect to ESP32 **5V/VIN** (requires 5V logic for optimal screen contrast)
* **GND:** Connect to ESP32 **GND**
* **SDA:** Connect to ESP32 **GPIO 21**
* **SCL:** Connect to ESP32 **GPIO 22**

### I2C Bus Scan
* On boot, the firmware automatically scans the I2C bus.
* It autodetects whether the display address is `0x27` or `0x3F`. If not found, it outputs warning scanner diagnostics to the Serial Monitor.

---

## 🛠️ Software Stack & Dependencies

The project uses **PlatformIO** for dependency management, build processes, and flashing:

* **Framework:** Arduino ESP32 framework (Core 2.0.x or newer)
* **Web Server:** `ESPAsyncWebServer` & `AsyncTCP` (Asynchronous HTTP/WebSocket core)
* **JSON Utility:** `ArduinoJson` (For configuration serialization and REST payloads)
* **Display Control:** `LiquidCrystal_I2C` (Low-level PCF8574 mappings)
* **Time Sync:** `NTPClient` (Internet time synchronization)
* **Network Diagnostics:** `ESP32Ping` (Checks internet status)
* **OTA Updates:** ElegantOTA (On-the-fly firmware upgrades at `/update` in async mode)

---

## 📂 Project Architecture

```
.
├── data/                       # Filesystem partition (LittleFS)
│   ├── index.html              # Glassmorphic single page dashboard
│   ├── style.css               # SaaS-style CSS (animations, themes, gradients)
│   ├── script.js               # Event loop, WebSocket, Speech API, and Local storage
│   ├── manifest.json           # PWA standalone application declarations
│   └── service-worker.js       # Offline cache control routines
├── src/                        # C++ Source Files
│   ├── config.h                # Pin maps, watchdog configurations, default credentials
│   ├── main.cpp                # Boot flow, RTOS loops, and watchdogs
│   ├── logger.h / .cpp         # Colored console logs, rotation, and profiling
│   ├── storage.h / .cpp        # LittleFS mounting, Preferences, and rolling history arrays
│   ├── wifi_manager.h / .cpp   # Hotspot configuration portal, background recovery loops
│   ├── time_manager.h / .cpp   # NTP translation and clock formatting
│   ├── lcd_manager.h / .cpp    # Custom character emoji mapping, scroll buffers, scan
│   ├── dashboard.h / .cpp      # Telemetry JSON aggregations (CPU, Temp, Uptime, Heap)
│   └── web_server.h / .cpp     # WebSocket callbacks, API routes, basic auth
├── platformio.ini              # Compiler environments and dependency declarations
└── min_spiffs.csv              # Custom flash partition definition (1.9MB App, 1.9MB LittleFS)
```

---

## 🚀 Build & Deployment Instructions

### 1. Set Up Hardware
Ensure your ESP32 is wired to the LCD module correctly (SDA=GPIO21, SCL=GPIO22, VCC=5V).

### 2. Configure Local Wi-Fi (Optional)
If you wish to configure credentials beforehand, edit `src/config.h`:
```cpp
#define DEFAULT_WIFI_SSID   "Your-WiFi-Name"
#define DEFAULT_WIFI_PASS   "Your-WiFi-Password"
```
*If credentials are left blank or connection fails, the ESP32 automatically starts a configuration Hotspot named `ESP32-Smart-LCD-XXXXXX` (Password: `admin123`). Connect and navigate to `http://192.168.4.1`.*

### 3. Compile and Upload Firmware
Run the following tasks using the PlatformIO CLI or IDE:
```bash
# Compile and build binaries
pio run

# Flash compiled firmware to the ESP32
pio run --target upload
```

### 4. Upload Filesystem Image (LittleFS)
You **must** upload the web assets in the `data/` folder to the ESP32's SPIFFS/LittleFS flash partition:
```bash
# Compile web assets into a LittleFS binary and upload
pio run --target uploadfs
```

---

## 📡 REST API Catalog

All endpoints require Basic Authentication if configured (default user: `admin`, default password: `password`).

| Endpoint | Method | Description | Body Params / Format |
|---|---|---|---|
| `/api/info` | `GET` | Fetch real-time hardware telemetry and memory gauges | None (JSON response) |
| `/api/history` | `GET` | Fetch rolling message histories (capped to 100 entries) | None (JSON response) |
| `/api/history` | `POST` | Import exported JSON logs list to local preferences | JSON Array string |
| `/api/settings` | `GET` | Fetch active device configurations | None (JSON response) |
| `/api/settings` | `POST` | Update hardware configurations (Hostname, Wi-Fi, clock formatting) | Form Data / URL encoded |
| `/api/text` | `POST` | Print custom text directly onto the LCD screen | `text`, `center` (bool), `scroll` (bool), `sender` |
| `/api/clear` | `POST` | Clear the LCD screen. Supports URL query `?history=true` | None (JSON response) |
| `/api/backlight` | `POST` | Toggle screen backlight state | `state` (`true`/`false`) |
| `/api/reboot` | `POST` | Triggers a remote software restart on the ESP32 | None |
| `/api/logs` | `GET` | Returns full console execution logs stored on LittleFS | None (JSON response) |

---

## 💬 Special Features & Interaction

### 1. Emoji Mapping
Due to the HD44780 standard character limitations, the LCD manager loads custom 8-pixel maps for emojis. The frontend intercepts and maps standard UTF-8 characters to character indexes:
* ❤️ = Custom Character `0` (coded as `\x08` to bypass string termination)
* 👍 = Custom Character `1`
* 😊 = Custom Character `2`
* 🔥 = Custom Character `3`
* ⭐ = Custom Character `4`

### 2. Speech-to-Text Voice Typing
The dashboard uses the browser's native **Web Speech API** (`webkitSpeechRecognition`). Pressing the mic icon on the dashboard captures spoken commands, limits them to the 32-character buffer, and pushes them instantly via WebSocket.

### 3. Progressive Web App (PWA)
The dashboard is installable as a standalone app on iOS, Android, and desktop operating systems. The service worker caches key assets (`index.html`, `style.css`, `script.js`) locally, allowing it to load instantly even during offline states or reboots.
