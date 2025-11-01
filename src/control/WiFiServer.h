#ifndef WIFI_SERVER_H
#define WIFI_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <functional>
#include "../config/Constants.h"

// WiFi and web server management
class WiFiServer {
private:
  AsyncWebServer* server;
  const char* ssid;
  const char* password;
  std::function<void(String)> onCommand;
  std::function<String()> onGetStatus;
  const char* dashboardHTML;

public:
  WiFiServer(const char* ssidName, const char* pass, const char* htmlContent)
    : server(new AsyncWebServer(WiFiConfig::SERVER_PORT)),
      ssid(ssidName),
      password(pass),
      dashboardHTML(htmlContent) {}

  ~WiFiServer() {
    delete server;
  }

  // Set command callback
  void setCommandCallback(std::function<void(String)> callback) {
    onCommand = callback;
  }

  // Set status callback
  void setStatusCallback(std::function<String()> callback) {
    onGetStatus = callback;
  }

  // Initialize WiFi hotspot
  bool begin() {
    Serial.println("🔧 Starting WiFi setup...");
    WiFi.mode(WIFI_AP);
    delay(100);

    bool result = WiFi.softAP(ssid, password);
    Serial.println("WiFi softAP result: " + String(result));

    if (result) {
      delay(500);
      IPAddress IP = WiFi.softAPIP();
      Serial.print("📡 Hotspot SSID: ");
      Serial.println(ssid);
      Serial.print("🔑 Password: ");
      Serial.println(password);
      Serial.print("🌐 IP Address: ");
      Serial.println(IP);

      setupRoutes();
      server->begin();
      Serial.println("✅ Web server started!");
      return true;
    }

    Serial.println("❌ WiFi hotspot failed!");
    return false;
  }

  // Setup web server routes
  void setupRoutes() {
    // Serve dashboard HTML
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request){
      request->send(200, "text/html", dashboardHTML);
    });

    // Status endpoint
    server->on("/status", HTTP_GET, [this](AsyncWebServerRequest *request){
      if (onGetStatus) {
        String status = onGetStatus();
        request->send(200, "application/json", status);
      } else {
        request->send(500, "application/json", "{\"error\":\"no status handler\"}");
      }
    });

    // Command endpoint
    server->on("/command", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        String body = String((char*)data).substring(0, len);

        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, body);

        if (!error) {
          String command = doc["command"];

          if (onCommand) {
            onCommand(command);
            request->send(200, "application/json", "{\"status\":\"ok\"}");
          } else {
            request->send(500, "application/json", "{\"error\":\"no command handler\"}");
          }
        } else {
          request->send(400, "application/json", "{\"error\":\"invalid json\"}");
        }
      });

    // 404 handler
    server->onNotFound([](AsyncWebServerRequest *request){
      request->send(404, "text/plain", "Not found");
    });
  }

  // Get connection info
  IPAddress getIP() const {
    return WiFi.softAPIP();
  }

  int getClientCount() const {
    return WiFi.softAPgetStationNum();
  }

  String getSSID() const {
    return String(ssid);
  }

  void printInfo() const {
    Serial.println("📊 WiFi Status:");
    Serial.print("  SSID: ");
    Serial.println(ssid);
    Serial.print("  IP: ");
    Serial.println(getIP());
    Serial.print("  Clients: ");
    Serial.println(getClientCount());
  }
};

#endif // WIFI_SERVER_H
