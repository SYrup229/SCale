#pragma once
#include "FoodManager.h"

// External shared state
extern DailyNutrition dailyTotals;
extern bool needDisplayUpdate;
extern FoodItem currentFood;
extern float lastGrams;
extern String lastTimestamp;
extern String lastMode; // optional, if used in display

inline void resetDailyTotals() {
    dailyTotals = {0, 0, 0, 0};
    currentFood.name = "";
    currentFood.calories = 0;
    currentFood.protein = 0;
    currentFood.carbs = 0;
    currentFood.fat = 0;
    needDisplayUpdate = true;
}
