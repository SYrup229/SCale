#pragma once
#include "FoodManager.h"

// External shared state
extern DailyNutrition dailyTotals;
extern bool needDisplayUpdate;
extern FoodItem* currentFood;

inline void resetDailyTotals() {
    dailyTotals = {0, 0, 0, 0};
    currentFood = nullptr;
    needDisplayUpdate = true;
}
