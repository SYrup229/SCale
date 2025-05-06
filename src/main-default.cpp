#include <Arduino.h>
#include "FoodManager.h"
#include "DisplayManager.h"
#include "BLEManager.h"
#include "WebServerManager.h"
#include "WebSocketManager.h"
#include "Utils.h"

// Wi-Fi credentials
const char* ssid = "Mihaita";
const char* password = "ciolan229";
const int SD_CS = 10;

// Globals
float weight = 1000.0;  // initial weight (simulate 1kg)
bool timeSynced = false;
bool needDisplayUpdate = true;
DailyNutrition dailyTotals = {0, 0, 0, 0};
FoodItem* currentFood = nullptr;

// Managers
FoodManager foodManager;
DisplayManager displayManager;
BLEManager bleManager;
WebServerManager webServerManager;
WebSocketManager webSocketManager;

void setup() {
  Serial.begin(115200);
  delay(200);

  // Initialize hardware
  displayManager.begin();
  foodManager.begin(SD_CS);
  bleManager.begin();
  webServerManager.begin(ssid, password);
  webSocketManager.begin();
  foodManager.begin(SD_CS);

  resetDailyTotals();
}

void loop() {
  webServerManager.handle();
  webSocketManager.handle(weight);

  if (!timeSynced && time(nullptr) > 24 * 3600) {
    Serial.println("✅ Time synchronized!");
    timeSynced = true;
  }

  if (needDisplayUpdate) {
    String ip = webServerManager.getDeviceIP().toString();
    String mode = webServerManager.getCurrentMode() == MODE_STA ? "STA" : "AP";
    displayManager.updateDisplay(weight, currentFood, dailyTotals, ip, mode);       
    needDisplayUpdate = false;
  }

  delay(50);
}

