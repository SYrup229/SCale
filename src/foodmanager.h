#pragma once
#include "esp_mac.h"
#include <Arduino.h>
#include <vector>
#include <map>
#include <sqlite3.h>

struct FoodItem {
    int id;
    String name;
    float calories;   
    float protein;
    float carbs;
    float fat;
    int usageCount = 0;  // ✅ If analyzeFoodLog is still in use
};

struct DailyNutrition {
  float calories, protein, carbs, fat;
};


class FoodManager {
public:
DailyNutrition dailyTotals;
sqlite3* getDatabaseHandle() { return db; }
void begin(int sdCsPin);
  void loadDatabase();
  void addFood(const String& name, float calories, float protein, float carbs, float fat);
  bool deleteFood(const String& name);
  std::vector<FoodItem>& getDatabase();

void restoreDailyTotalsFromDatabase();

void loadColorMap();
String getColorForFood(const String& name);


private:
 std::vector<FoodItem> foodDatabase;
    std::map<String, String> foodColorMap;
    sqlite3* db = nullptr;  // ✅ DB handle here
};
