#include "esp_mac.h"
#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "FoodManager.h"

class WebServerManager {
public:
    void begin(const char* ssid, const char* password);
    void handle();
    

private:
    WebServer server = WebServer(80);

    void handleRoot();
    void handleAddFood();
    void handleDeleteFood();
    void handleSelect();
    void handleReset();
    
    void startWiFi(const char* ssid, const char* password);
    void syncTime();
};
