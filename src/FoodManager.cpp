#include "FoodManager.h"
#include <SD.h>

void FoodManager::begin() {
  if (!SD.begin(10)) {  // SD_CS pin
    Serial.println("❌ SD init failed");
    return;
  }
  Serial.println("✅ SD initialized");
  loadDatabase();
  analyzeFoodLog();

  if (!SD.exists(logFileName)) {
    File f = SD.open(logFileName, FILE_WRITE);
    if (f) {
      f.println("DateTime,Weight(g),Food,Calories,Protein,Carbs,Fat");
      f.close();
    }
  }
}

void FoodManager::loadDatabase() {
  if (!SD.exists(foodDBFileName)) {
    Serial.println("⚠️ food_db.csv missing");
    return;
  }

  File f = SD.open(foodDBFileName, FILE_READ);
  if (!f) {
    Serial.println("⚠️ Cannot open food_db.csv");
    return;
  }

  foodDatabase.clear();
  bool first = true;
  int order = 0;
  Serial.println("Loading food database...");
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) continue;
    if (first && line.startsWith("Name")) { first = false; continue; }
    first = false;

    int start = 0, idx;
    std::vector<String> cols;
    for (int i = 0; i < 4; ++i) {
      idx = line.indexOf(',', start);
      if (idx == -1) break;
      cols.push_back(line.substring(start, idx));
      start = idx + 1;
    }
    cols.push_back(line.substring(start));
    
    if (cols.size() < 5) continue;

    foodDatabase.push_back({
      cols[0],
      cols[1].toFloat(),
      cols[2].toFloat(),
      cols[3].toFloat(),
      cols[4].toFloat(),
      order++,  // addedOrder
      0         // usageCount (to be filled later)
    });
    Serial.println(" • " + cols[0]);
  }
  f.close();
  Serial.printf("✅ %d items loaded\n", (int)foodDatabase.size());
}



void FoodManager::analyzeFoodLog() {
  if (!SD.exists(logFileName)) {
    Serial.println("⚠️ No food_log.csv yet, skipping usage analysis");
    return;
  }

  File f = SD.open(logFileName, FILE_READ);
  if (!f) {
    Serial.println("⚠️ Cannot open food_log.csv");
    return;
  }

  bool first = true;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) continue;
    if (first && line.startsWith("DateTime")) { first = false; continue; }
    first = false;

    int start = 0, idx;
    idx = line.indexOf(',', start); start = idx + 1;
    idx = line.indexOf(',', start); start = idx + 1;
    idx = line.indexOf(',', start);
    String foodName = line.substring(start, idx);
    foodName.trim();

    for (auto& item : foodDatabase) {
      if (item.name.equalsIgnoreCase(foodName)) {
        item.usageCount++;
        break;
      }
    }
  }
  f.close();
  Serial.println("✅ Food usage counts updated");
}

void FoodManager::addFood(const String& name, float calories, float protein, float carbs, float fat) {
  File f = SD.open(foodDBFileName, FILE_APPEND);
  if (!f) {
    Serial.println("❌ Failed to open food_db.csv for appending");
    return;
  }

  f.println(name + "," + String(calories, 2) + "," + String(protein, 2) + "," +
            String(carbs, 2) + "," + String(fat, 2));
  f.close();

  foodDatabase.push_back({
    name,
    calories,
    protein,
    carbs,
    fat,
    (int)foodDatabase.size(),  // next order
    0
  });
}

bool FoodManager::deleteFood(const String& target) {
  bool found = false;
  std::vector<FoodItem> newDB;
  for (auto& item : foodDatabase) {
    if (!item.name.equalsIgnoreCase(target)) {
      newDB.push_back(item);
    } else {
      found = true;
    }
  }

  if (!found) return false;

  File f = SD.open(foodDBFileName, FILE_WRITE);
  if (!f) {
    Serial.println("❌ Failed to rewrite food_db.csv");
    return false;
  }

  f.println("Name,Calories,Protein,Carbs,Fat");
  for (auto& item : newDB) {
    f.println(item.name + "," + String(item.calories, 2) + "," +
              String(item.protein, 2) + "," +
              String(item.carbs, 2) + "," +
              String(item.fat, 2));
  }
  f.close();

  foodDatabase = newDB;
  return true;
}

std::vector<FoodItem>& FoodManager::getDatabase() {
  return foodDatabase;
}
