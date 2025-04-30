#pragma once
#include "esp_mac.h"
#include <Arduino.h>
#include <vector>
#include <map>

struct FoodItem {
  String name;
  float calories, protein, carbs, fat;
  int addedOrder;
  int usageCount;
};

struct DailyNutrition {
  float calories, protein, carbs, fat;
};

class FoodManager {
public:
void begin(int sdCsPin);
  void loadDatabase();
  void analyzeFoodLog();
  void addFood(const String& name, float calories, float protein, float carbs, float fat);
  bool deleteFood(const String& name);
  std::vector<FoodItem>& getDatabase();

  // ✅ In-memory mapping of food name → color name
  std::map<String, String> foodColorMap;

  void loadColorMapFromSD();
  void saveColorMapToSD();


private:
  const char* foodDBFileName = "/food_db.csv";
  const char* logFileName    = "/food_log.csv";
  std::vector<FoodItem> foodDatabase;
};
