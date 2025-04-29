#include "esp_mac.h"
#pragma once
#include <Arduino.h>
#include <vector>



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
  void begin();
  void loadDatabase();
  void analyzeFoodLog();
  void addFood(const String& name, float calories, float protein, float carbs, float fat);
  bool deleteFood(const String& name);
  std::vector<FoodItem>& getDatabase();
  
private:
  const char* foodDBFileName = "/food_db.csv";
  const char* logFileName = "/food_log.csv";
  std::vector<FoodItem> foodDatabase;
};
