#include <Arduino.h>
#include "FoodManager.h"
#include "DisplayManager.h"
#include "BLEManager.h"
#include "WebServerManager.h"
#include "WebSocketManager.h"
#include "Utils.h"
#include "Scale_LoadCell.h"
#include "Color_Sensor.h"

// Wi-Fi credentials
const char* ssid = "TP-Link_D358";
const char* password = "25194432";
const int SD_CS = 10;

// Globals
float weight = 1000.0;  // initial weight (simulate 1kg)
bool tareScale = false;
bool timeSynced = false;
bool needDisplayUpdate = true;
DailyNutrition dailyTotals = {0, 0, 0, 0};
FoodItem currentFood;

// Color sensor
DFRobot_AS7341 colorSensor;
colorSensorState colorState = INACTIVE;

String lastTimestamp = "";
String lastMode = "";

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
  webServerManager.begin(ssid, password, foodManager.getDatabaseHandle());
  webSocketManager.begin();
  foodManager.begin(SD_CS);

  initSpectralSensor(colorSensor);

  scale_setup();
  resetDailyTotals();
}

void loop() {

  weight = scale_getWeight();

  webServerManager.handle();
  webSocketManager.handle(weight, tareScale);

  if (!timeSynced && time(nullptr) > 24 * 3600) {
    Serial.println("✅ Time synchronized!");
    timeSynced = true;
  }

  if (needDisplayUpdate) {
    String ip = webServerManager.getDeviceIP().toString();
    String mode = webServerManager.getCurrentMode() == MODE_STA ? "STA" : "AP";
    displayManager.updateDisplay(weight, &currentFood, dailyTotals, ip, mode);
    needDisplayUpdate = false;
  }

  delay(50);
}

