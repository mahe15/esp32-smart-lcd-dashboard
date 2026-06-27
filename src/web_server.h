#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

class WebServerManager {
public:
    static void init();
    static void handle();
    static void broadcastTelemetry();
    static void broadcastLogs(const String& logMsg);

private:
    static void setupRoutes();
    static void setupWebSockets();
    static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);
    static void handleWsMessage(AsyncWebSocketClient* client, const String& message);
    static bool isAuthenticated(AsyncWebServerRequest* request);
    static void sendJsonError(AsyncWebServerRequest* request, int code, const String& message);

    static AsyncWebServer server;
    static AsyncWebSocket ws;
    static unsigned long lastBroadcastTime;
};

#endif // WEB_SERVER_H
