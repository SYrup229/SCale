#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "FoodManager.h"
#include "ColorMap.h"

// ✅ Define enum before the class so all scopes can see it
enum WiFiModeType { MODE_STA, MODE_AP };

class WebServerManager {
public:
    void begin(const char* ssid, const char* password);
    void handle();

    // ✅ Add these accessors
    WiFiModeType getCurrentMode();
    IPAddress getDeviceIP();

    void logSpectrumEntry(const String& foodName, float grams, const String& color, const Spectrum& spectrum);
    void logFoodEntry(const String& foodName, float grams, float cal, float prot, float carbs, float fat);

private:
    WebServer server = WebServer(80);
    WiFiModeType currentMode = MODE_STA;  // ✅ Store mode

    void handleRoot();
    void handleAddFood();
    void handleDeleteFood();
    void handleSelect();
    void handleReset();
    void startWiFi(const char* ssid, const char* password);
    void syncTime();
    bool checkAuth();
};
