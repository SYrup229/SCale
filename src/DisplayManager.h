#pragma once

#include <TFT_eSPI.h>
#include "Utils.h"

class DisplayManager {
public:
    void begin();
    void updateDisplay(float weight, FoodItem* currentFood, DailyNutrition& totals, const String& ip = "");

private:
    TFT_eSPI tft;
};
