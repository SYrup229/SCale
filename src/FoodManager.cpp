#include "FoodManager.h"
#include <SD.h>

#include <sqlite3.h>

sqlite3* db;  // Add this at the top or as a class member
sqlite3_stmt* stmt;
extern DailyNutrition dailyTotals;
extern FoodItem currentFood;

void FoodManager::addFood(const String& name, float calories, float protein, float carbs, float fat) {
    const char* sql = "INSERT INTO Food (name, calories, protein, carbs, fat) VALUES (?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 2, calories);
        sqlite3_bind_double(stmt, 3, protein);
        sqlite3_bind_double(stmt, 4, carbs);
        sqlite3_bind_double(stmt, 5, fat);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            Serial.println("❌ Failed to insert food item.");
        } else {
            Serial.println("✅ Food inserted into database.");
        }

        sqlite3_finalize(stmt);
    } else {
        Serial.printf("❌ Failed to prepare SQL insert: %s\n", sqlite3_errmsg(db));
    }
  }

void FoodManager::begin(int sdCsPin) {
    if (!SD.begin(sdCsPin)) {
        Serial.println("❌ SD card init failed.");
        return;
    }

    if (sqlite3_open("/sd/food.db", &db) != SQLITE_OK) {
        Serial.println("❌ Cannot open SQLite database.");
        return;
    }

    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    const char* initSQL = R"(
    PRAGMA foreign_keys = ON;

    CREATE TABLE IF NOT EXISTS Food (
        food_id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        calories REAL,
        protein REAL,
        carbs REAL,
        fat REAL
    );

    CREATE TABLE IF NOT EXISTS LogEntry (
        log_id INTEGER PRIMARY KEY AUTOINCREMENT,
        food_id INTEGER NOT NULL,
        grams REAL NOT NULL,
        timestamp TEXT NOT NULL,
        FOREIGN KEY (food_id) REFERENCES Food(food_id) ON DELETE CASCADE
    );

    CREATE TABLE IF NOT EXISTS ColorMap (
        food_id INTEGER PRIMARY KEY,
        color_name TEXT,
        FOREIGN KEY (food_id) REFERENCES Food(food_id) ON DELETE CASCADE
    );
)";

    char* errMsg = nullptr;
    if (sqlite3_exec(db, initSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        Serial.printf("❌ SQL init error: %s\n", errMsg);
        sqlite3_free(errMsg);
    } else {
        Serial.println("✅ SQLite schema initialized.");
    }

    restoreDailyTotalsFromDatabase();
int count = 0;
sqlite3_exec(db, "SELECT COUNT(*) FROM Food;", [](void* data, int argc, char** argv, char**) -> int {
    if (argc > 0 && argv[0]) *(int*)data = atoi(argv[0]);
    return 0;
}, &count, nullptr);

if (count == 0 && SD.exists("/food_db.csv")) {
    Serial.println("⚙️ Importing old food_db.csv into SQLite...");
    File f = SD.open("/food_db.csv");
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("Name")) continue;

        // Split CSV line
        std::vector<String> parts;
        while (line.length()) {
            int idx = line.indexOf(',');
            if (idx == -1) {
                parts.push_back(line);
                break;
            }
            parts.push_back(line.substring(0, idx));
            line = line.substring(idx + 1);
        }

        if (parts.size() >= 5) {
            const char* sql = "INSERT INTO Food (name, calories, protein, carbs, fat) VALUES (?, ?, ?, ?, ?);";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, parts[0].c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_double(stmt, 2, parts[1].toFloat());
                sqlite3_bind_double(stmt, 3, parts[2].toFloat());
                sqlite3_bind_double(stmt, 4, parts[3].toFloat());
                sqlite3_bind_double(stmt, 5, parts[4].toFloat());
                if (sqlite3_step(stmt) == SQLITE_DONE) {
                    Serial.printf("✅ Imported: %s\n", parts[0].c_str());
                } else {
                    Serial.printf("❌ Insert failed: %s\n", sqlite3_errmsg(db));
                }
                sqlite3_finalize(stmt);
            }
        }
    }
    f.close();
}

}

void FoodManager::restoreDailyTotalsFromDatabase() {
    if (!db) return;

    const char* query = "SELECT SUM(calories), SUM(protein), SUM(carbs), SUM(fat) FROM LogEntry;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            dailyTotals.calories = sqlite3_column_double(stmt, 0);
            dailyTotals.protein  = sqlite3_column_double(stmt, 1);
            dailyTotals.carbs    = sqlite3_column_double(stmt, 2);
            dailyTotals.fat      = sqlite3_column_double(stmt, 3);
            Serial.println("✅ Restored daily totals from DB");
        } else {
            Serial.println("⚠️ No daily log data found");
        }
        sqlite3_finalize(stmt);
    } else {
        Serial.printf("❌ Failed to restore totals: %s\n", sqlite3_errmsg(db));
    }
}



void FoodManager::loadDatabase() {
    foodDatabase.clear();

    const char* selectSQL = "SELECT food_id, name, calories, protein, carbs, fat FROM Food;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, selectSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            FoodItem item;
            item.id      = sqlite3_column_int(stmt, 0);
            item.name    = String(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
            item.calories = sqlite3_column_double(stmt, 2);
            item.protein = sqlite3_column_double(stmt, 3);
            item.carbs   = sqlite3_column_double(stmt, 4);
            item.fat     = sqlite3_column_double(stmt, 5);
            foodDatabase.push_back(item);
        }
        sqlite3_finalize(stmt);
        Serial.printf("✅ Loaded %d food items from database.\n", foodDatabase.size());
    } else {
        Serial.println("❌ Failed to prepare SELECT in loadDatabase()");
    }
}


std::map<String, String> foodColorMap;

void FoodManager::loadColorMap() {
    foodColorMap.clear();

    const char* query = R"(SELECT Food.name, ColorMap.color_name
                           FROM ColorMap
                           JOIN Food ON Food.food_id = ColorMap.food_id;)";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            String foodName = String(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
            String color = String(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
            foodColorMap[foodName] = color;
        }
        sqlite3_finalize(stmt);
        Serial.printf("✅ Loaded %d food-color mappings.\n", foodColorMap.size());
    } else {
        Serial.println("❌ Failed to load color map from SQL.");
    }
}



std::vector<FoodItem>& FoodManager::getDatabase() {
  return foodDatabase;
}
