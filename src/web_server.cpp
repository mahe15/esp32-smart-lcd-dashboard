#include "web_server.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include "logger.h"
#include "storage.h"
#include "time_manager.h"
#include "wifi_manager.h"
#include "lcd_manager.h"
#include "dashboard.h"

AsyncWebServer WebServerManager::server(HTTP_PORT);
AsyncWebSocket WebServerManager::ws(WS_PATH);
unsigned long WebServerManager::lastBroadcastTime = 0;

void WebServerManager::init() {
    setupWebSockets();
    setupRoutes();
    
    // Register ElegantOTA with Admin credentials authentication
    ElegantOTA.begin(&server, HTTP_ADMIN_USER, HTTP_ADMIN_PASS);
    Logger::log(LOG_SUCCESS, "OTA", "ElegantOTA initialized at /update (Admin authenticated).");
    
    server.begin();
    Logger::log(LOG_SUCCESS, "HTTP", "Web Server started on port %d.", HTTP_PORT);
}

void WebServerManager::handle() {
    ws.cleanupClients();
    
    // Periodic telemetry broadcast (every 1 second)
    unsigned long now = millis();
    if (now - lastBroadcastTime > TELEMETRY_INTERVAL_MS) {
        lastBroadcastTime = now;
        broadcastTelemetry();
    }
}

void WebServerManager::setupWebSockets() {
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    Logger::log(LOG_INFO, "WS", "WebSocket endpoint registered at %s", WS_PATH);
}

void WebServerManager::onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Logger::log(LOG_SUCCESS, "WS", "Client connected: ID %u IP: %s", client->id(), client->remoteIP().toString().c_str());
        
        // Send initial system configuration immediately on connect
        String telemetry = Dashboard::getTelemetryJson(server->count());
        client->text(telemetry);
        
        // Send recent logs
        String logs = "{\"type\":\"logs\",\"data\":" + Logger::getLogsJson() + "}";
        client->text(logs);
        
        // Broadcast new client count to others
        ws.textAll("{\"type\":\"client_count\",\"count\":" + String(server->count()) + "}");
        
    } else if (type == WS_EVT_DISCONNECT) {
        Logger::log(LOG_INFO, "WS", "Client disconnected: ID %u", client->id());
        ws.textAll("{\"type\":\"client_count\",\"count\":" + String(server->count()) + "}");
        
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->opcode == WS_TEXT) {
            String msg = "";
            for (size_t i = 0; i < len; i++) {
                msg += (char)data[i];
            }
            handleWsMessage(client, msg);
        }
    }
}

void WebServerManager::handleWsMessage(AsyncWebSocketClient* client, const String& message) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        Logger::log(LOG_ERROR, "WS", "Parsing WebSocket JSON failed: %s", error.c_str());
        return;
    }
    
    String type = doc["type"];
    
    if (type == "text") {
        String text = doc["text"];
        bool center = doc["center"] | false;
        bool scroll = doc["scroll"] | false;
        
        // Print on LCD
        LCDManager::printText(text, center, scroll);
        
        // Add to history (only if not typing/incomplete text)
        bool isDone = doc["done"] | false;
        String trimmedText = text;
        trimmedText.trim();
        if (isDone && trimmedText.length() > 0) {
            Storage::addHistoryItem(trimmedText, "Web Interface");
        }
        
        // Broadcast update to all clients except sender to keep cursor/typing sync
        String wsUpdate = "{\"type\":\"lcd_update\",\"text\":\"" + text + "\",\"center\":" + (center ? "true" : "false") + ",\"scroll\":" + (scroll ? "true" : "false") + "}";
        ws.textAll(wsUpdate);
        
    } else if (type == "clear") {
        LCDManager::clear();
        ws.textAll("{\"type\":\"lcd_clear\"}");
        
    } else if (type == "backlight") {
        bool state = doc["state"];
        LCDManager::setBacklight(state);
        ws.textAll("{\"type\":\"backlight_update\",\"state\":" + String(state ? "true" : "false") + "}");
        
    } else if (type == "display") {
        bool state = doc["state"];
        LCDManager::setDisplay(state);
        ws.textAll("{\"type\":\"display_update\",\"state\":" + String(state ? "true" : "false") + "}");
    }
}

void WebServerManager::broadcastTelemetry() {
    if (ws.count() > 0) {
        String telemetry = Dashboard::getTelemetryJson(ws.count());
        ws.textAll(telemetry);
    }
}

void WebServerManager::broadcastLogs(const String& logMsg) {
    if (ws.count() > 0) {
        // Wrap log line in json object
        String escapedLine = logMsg;
        escapedLine.replace("\\", "\\\\");
        escapedLine.replace("\"", "\\\"");
        String logJson = "{\"type\":\"log_line\",\"data\":\"" + escapedLine + "\"}";
        ws.textAll(logJson);
    }
}

bool WebServerManager::isAuthenticated(AsyncWebServerRequest* request) {
    if (!request->authenticate(HTTP_ADMIN_USER, HTTP_ADMIN_PASS)) {
        request->requestAuthentication();
        return false;
    }
    return true;
}

void WebServerManager::sendJsonError(AsyncWebServerRequest* request, int code, const String& message) {
    String response = "{\"status\":\"error\",\"message\":\"" + message + "\"}";
    request->send(code, "application/json", response);
}

void WebServerManager::setupRoutes() {
    // API GET Info
    server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest* request) {
        unsigned long start = millis();
        String response = Dashboard::getTelemetryJson(ws.count());
        request->send(200, "application/json", response);
        Logger::logRequest(request->client()->remoteIP().toString().c_str(), "/api/info", millis() - start, ESP.getFreeHeap(), 200);
    });
    
    // API GET History
    server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest* request) {
        unsigned long start = millis();
        String response = Storage::getHistoryJson();
        request->send(200, "application/json", response);
        Logger::logRequest(request->client()->remoteIP().toString().c_str(), "/api/history", millis() - start, ESP.getFreeHeap(), 200);
    });
    
    // API GET Settings
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest* request) {
        unsigned long start = millis();
        SystemSettings settings = Storage::loadSettings();
        StaticJsonDocument<512> doc;
        doc["darkMode"] = settings.darkMode;
        doc["accentColor"] = settings.accentColor;
        doc["scrollSpeed"] = settings.scrollSpeed;
        doc["is24Hour"] = settings.is24Hour;
        doc["backlightOn"] = settings.backlightOn;
        doc["displayOn"] = settings.displayOn;
        doc["hostname"] = settings.hostname;
        doc["notificationsEnabled"] = settings.notificationsEnabled;
        doc["wifiSsid"] = settings.wifiSsid;
        doc["useStaticIp"] = settings.useStaticIp;
        doc["staticIp"] = settings.staticIp;
        doc["gateway"] = settings.gateway;
        doc["subnet"] = settings.subnet;
        doc["dns"] = settings.dns;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
        Logger::logRequest(request->client()->remoteIP().toString().c_str(), "/api/settings", millis() - start, ESP.getFreeHeap(), 200);
    });
    
    // API POST Text update
    server.on("/api/text", HTTP_POST, [](AsyncWebServerRequest* request) {
        unsigned long start = millis();
        String text = "";
        bool center = false;
        bool scroll = false;
        String sender = "API Client";
        
        if (request->hasParam("text", true)) {
            text = request->getParam("text", true)->value();
        }
        if (request->hasParam("center", true)) {
            center = request->getParam("center", true)->value() == "true";
        }
        if (request->hasParam("scroll", true)) {
            scroll = request->getParam("scroll", true)->value() == "true";
        }
        if (request->hasParam("sender", true)) {
            sender = request->getParam("sender", true)->value();
        }
        
        if (text.length() == 0) {
            sendJsonError(request, 400, "Missing required 'text' parameter.");
            Logger::logRequest(request->client()->remoteIP().toString().c_str(), "/api/text", millis() - start, ESP.getFreeHeap(), 400);
            return;
        }
        
        LCDManager::printText(text, center, scroll);
        Storage::addHistoryItem(text, sender);
        
        // Broadcast telemetry update
        ws.textAll("{\"type\":\"lcd_update\",\"text\":\"" + text + "\",\"center\":" + (center ? "true" : "false") + ",\"scroll\":" + (scroll ? "true" : "false") + "}");
        
        request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"LCD text updated\"}");
        Logger::logRequest(request->client()->remoteIP().toString().c_str(), "/api/text", millis() - start, ESP.getFreeHeap(), 200);
    });
    
    // API POST Clear
    server.on("/api/clear", HTTP_POST, [](AsyncWebServerRequest* request) {
        unsigned long start = millis();
        bool clearHistory = false;
        if (request->hasParam("history")) {
            clearHistory = request->getParam("history")->value() == "true";
        }
        
        if (clearHistory) {
            Storage::clearHistory();
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"History cleared\"}");
        } else {
            LCDManager::clear();
            ws.textAll("{\"type\":\"lcd_clear\"}");
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"LCD cleared\"}");
        }
        Logger::logRequest(request->client()->remoteIP().toString().c_str(), "/api/clear", millis() - start, ESP.getFreeHeap(), 200);
    });

    // API POST Import History
    server.on("/api/history", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            // Main handler does nothing; waiting for body completion
        }, 
        NULL, 
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (index == 0) {
                request->_tempObject = malloc(total + 1);
                if (request->_tempObject != NULL) {
                    ((char*)request->_tempObject)[total] = '\0';
                }
            }
            if (request->_tempObject != NULL) {
                memcpy((char*)request->_tempObject + index, data, len);
            }
            if (index + len == total) {
                char* body = (char*)request->_tempObject;
                bool success = Storage::importHistory(String(body));
                free(request->_tempObject);
                request->_tempObject = NULL;
                if (success) {
                    request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"History imported\"}");
                } else {
                    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Failed to import history\"}");
                }
            }
        }
    );
    
    // API POST Backlight
    server.on("/api/backlight", HTTP_POST, [](AsyncWebServerRequest* request) {
        unsigned long start = millis();
        if (request->hasParam("state", true)) {
            bool val = request->getParam("state", true)->value() == "true" || request->getParam("state", true)->value() == "1";
            LCDManager::setBacklight(val);
            ws.textAll("{\"type\":\"backlight_update\",\"state\":" + String(val ? "true" : "false") + "}");
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Backlight state updated\"}");
        } else {
            sendJsonError(request, 400, "Missing 'state' parameter.");
        }
        Logger::logRequest(request->client()->remoteIP().toString().c_str(), "/api/backlight", millis() - start, ESP.getFreeHeap(), request->hasParam("state", true) ? 200 : 400);
    });
    
    // API POST Settings (Requires Auth)
    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest* request) {
        unsigned long start = millis();
        if (!isAuthenticated(request)) return;
        
        SystemSettings settings = Storage::loadSettings();
        
        if (request->hasParam("darkMode", true)) settings.darkMode = request->getParam("darkMode", true)->value() == "true";
        if (request->hasParam("accentColor", true)) settings.accentColor = request->getParam("accentColor", true)->value();
        if (request->hasParam("scrollSpeed", true)) settings.scrollSpeed = request->getParam("scrollSpeed", true)->value().toInt();
        if (request->hasParam("is24Hour", true)) {
            settings.is24Hour = request->getParam("is24Hour", true)->value() == "true";
            TimeManager::updateHourFormat(settings.is24Hour);
        }
        if (request->hasParam("hostname", true)) settings.hostname = request->getParam("hostname", true)->value();
        if (request->hasParam("notifs", true)) settings.notificationsEnabled = request->getParam("notifs", true)->value() == "true";
        
        // Wi-Fi configs
        if (request->hasParam("wifiSsid", true)) settings.wifiSsid = request->getParam("wifiSsid", true)->value();
        if (request->hasParam("wifiPass", true)) settings.wifiPassword = request->getParam("wifiPass", true)->value();
        if (request->hasParam("useStaticIp", true)) settings.useStaticIp = request->getParam("useStaticIp", true)->value() == "true";
        if (request->hasParam("staticIp", true)) settings.staticIp = request->getParam("staticIp", true)->value();
        if (request->hasParam("gateway", true)) settings.gateway = request->getParam("gateway", true)->value();
        if (request->hasParam("subnet", true)) settings.subnet = request->getParam("subnet", true)->value();
        if (request->hasParam("dns", true)) settings.dns = request->getParam("dns", true)->value();
        
        Storage::saveSettings(settings);
        request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Settings updated successfully\"}");
        Logger::logRequest(request->client()->remoteIP().toString().c_str(), "/api/settings", millis() - start, ESP.getFreeHeap(), 200);
    });
    
    // API POST Reboot (Requires Auth)
    server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* request) {
        unsigned long start = millis();
        if (!isAuthenticated(request)) return;
        
        Logger::log(LOG_WARNING, "SYSTEM", "Remote reboot request received. Restarting ESP32...");
        request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Device rebooting now...\"}");
        Logger::logRequest(request->client()->remoteIP().toString().c_str(), "/api/reboot", millis() - start, ESP.getFreeHeap(), 200);
        delay(1000);
        ESP.restart();
    });
    
    // API GET Logs (Requires Auth)
    server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest* request) {
        unsigned long start = millis();
        if (!isAuthenticated(request)) return;
        
        String logs = Logger::getLogsJson();
        request->send(200, "application/json", logs);
        Logger::logRequest(request->client()->remoteIP().toString().c_str(), "/api/logs", millis() - start, ESP.getFreeHeap(), 200);
    });
    
    // Serve Static HTML/CSS/JS files from LittleFS
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    
    // Handle NotFound (Fallback redirect to root)
    server.onNotFound([](AsyncWebServerRequest* request) {
        if (request->url().startsWith("/api/")) {
            sendJsonError(request, 404, "Endpoint not found");
        } else {
            request->redirect("/");
        }
    });
}
