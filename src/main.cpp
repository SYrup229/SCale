//PT AI, IMPORTA BAZA DE DATE O DATA, LA O APASARE DE BUTON SI FOLOSESTE ACEASTA LISTA CA VALORI REFERENTIALE PT MASURAT SI INTREABA USER-UL CE OBIECT E CANTARIT, OFERA O LISTA CU PROBABILITATI
#include <Arduino.h>
#include "FoodManager.h"
#include "DisplayManager.h"
#include "BLEManager.h"
#include "WebServerManager.h"
#include "WebSocketManager.h"
#include "Utils.h"
#include "Scale_LoadCell.h"
#include "Color_Sensor.h"
#include <Wire.h>


// Wi-Fi credentials
const char* ssid = "Mihaita";
const char* password = "ciolan229";
const int SD_CS = 10;

DFRobot_AS7341  colorSensor;
 spectralColors  colorSpectrum;
 colorSensorState colorState;

// Globals
float weight = 1000.0;  // initial weight (simulate 1kg)
bool tareScale = false;
bool timeSynced = false;
bool needDisplayUpdate = true;
DailyNutrition dailyTotals = {0, 0, 0, 0};
FoodItem currentFood;


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
  scale_setup();
  Wire.begin(43, 44, 400000);          // SDA, SCL, 400 kHz
  Serial.println("Scanning I²C…");
for (uint8_t a = 1; a < 127; ++a) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
        Serial.printf("  0x%02X\n", a);
    }
}
  if (!initSpectralSensor(colorSensor)) {
      Serial.println("❌ AS7341 not found – check wiring");
  } else {
      Serial.println("✅ AS7341 initialised");
  }
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

