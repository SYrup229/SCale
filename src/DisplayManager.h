#include "esp_mac.h"
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "FoodManager.h"

class DisplayManager {
public:
    void begin();
    void updateDisplay(float weight, FoodItem* currentFood, DailyNutrition& dailyTotals);

private:
    TFT_eSPI tft;
};
