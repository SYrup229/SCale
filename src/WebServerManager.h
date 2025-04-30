#include "esp_mac.h"
#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "FoodManager.h"
#include "ColorMap.h"

class WebServerManager {
public:
    void begin(const char* ssid, const char* password);
    void handle();
    void logSpectrumEntry(const String& foodName, float grams, const String& color, const Spectrum& spectrum);
    void logFoodEntry(const String& foodName, float grams, float cal, float prot, float carbs, float fat);


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
