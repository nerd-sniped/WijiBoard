#include "ServerLib.h"
#include <SPIFFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <FS.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void initWebSocket();

const char *ssid = "WijiBoard";
const char *password = "Planchette";
const char *hostname = "WijiBoard";

IPAddress local_ip(8, 8, 8, 8);
IPAddress gateway(8, 8, 8, 8);
IPAddress subnet(255, 255, 255, 0);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dnsServer;

extern bool newRequest;
extern char message;
extern bool isWordMode;           // Add new flag
extern String currentWord;        // Add word storage
extern int currentLetterIndex;    // Add letter index

void StartServer()
{
    Serial.println("StartServer() called");

    WiFi.softAPConfig(local_ip, gateway, subnet);
    /*WiFi.mode(WIFI_AP);*/
    bool apSuccess = WiFi.softAP(ssid);

    if (!SPIFFS.begin(true))
    {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return; // Don't continue if we can't initialize SPIFFS
    }
    else
    {
        Serial.println("SPIFFS mounted successfully");
    }

// Initialize mDNS
    if (!MDNS.begin(hostname)) {
        Serial.println("Error setting up MDNS responder!");
        return;
    }
    Serial.println("mDNS responder started");
    MDNS.addService("http", "tcp", 80);

    // Check if index.html exists
    if (SPIFFS.exists("/index.html"))
    {
        Serial.println("index.html exists");
    }
    else
    {
        Serial.println("index.html does not exist");
    }

    initWebSocket();

    // Default catch-all handler for all other requests to redirect to /index.html
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->redirect("/index.html");
    });

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        File file = SPIFFS.open("/index.html", "r");
        if (!file) {
            Serial.println("Failed to open index.html");
            request->send(500, "text/plain", "Failed to open index.html");
        } else {
            AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.html", "text/html");
            Serial.println("Sent " + String(file.size()) + " bytes from index.html");
            request->send(response);
            file.close();
        }
    });

    //server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (SPIFFS.exists("/style.css")) {
            Serial.println("style.css exists");
            request->send(SPIFFS, "/style.css", "text/css");
        } else {
            Serial.println("style.css does not exist");
            request->send(404, "text/plain", "style.css not found");
        }
    });

    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (SPIFFS.exists("/script.js")) {
            Serial.println("script.js exists");
            request->send(SPIFFS, "/script.js", "application/javascript");
        } else {
            Serial.println("script.js does not exist");
            request->send(404, "text/plain", "script.js not found");
        }
    });

    server.on("/Corner.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (SPIFFS.exists("/Corner.svg")) {
            Serial.println("Corner.svg exists");
            request->send(SPIFFS, "/Corner.svg", "image/svg+xml");
        } else {
            Serial.println("Corner.svg does not exist");
            request->send(404, "text/plain", "Corner.svg not found");
        }
    });

    server.on("/WijiTitleBlock.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (SPIFFS.exists("/WijiTitleBlock.svg")) {
            Serial.println("WijiTitleBlock.svg exists");
            request->send(SPIFFS, "/WijiTitleBlock.svg", "image/svg+xml");
        } else {
            Serial.println("WijiTitleBlock.svg does not exist");
            request->send(404, "text/plain", "WijiTitleBlock.svg not found");
        }
    });

    server.on("/SpecialElite.woff2", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (SPIFFS.exists("/SpecialElite.woff2")) {
            Serial.println("SpecialElite.woff2 exists");
            request->send(SPIFFS, "/SpecialElite.woff2", "font/woff2");
        } else {
            Serial.println("SpecialElite.woff2 does not exist");
            request->send(404, "text/plain", "SpecialElite.woff2 not found");
        }
    });

        server.begin();
        Serial.println("Server started");

}
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
        data[len] = 0;
        String receivedMessage = String((char*)data);
        
        // Check if it's a word sequence
        if (receivedMessage.startsWith("WORD:")) {
            currentWord = receivedMessage.substring(5); // Remove "WORD:" prefix
            currentLetterIndex = 0;
            isWordMode = true;
            newRequest = true;
            Serial.println("Word received: " + currentWord);
        } else if (len == 1) {
            // Single character (existing functionality)
            message = (char)data[0];
            isWordMode = false;
            newRequest = true;
            Serial.println("Single character: " + String(message));
        }
        
        ws.textAll(String((char*)data));
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
    case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
    case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
        break;
    }
}

void initWebSocket()
{
    ws.onEvent(onEvent);
    server.addHandler(&ws);
}