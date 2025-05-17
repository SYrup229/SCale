#include "WebServerManager.h"
#include "DisplayManager.h"
#include "WebSocketManager.h"
#include "FoodManager.h"
#include "ColorMap.h" 
#include <SD.h>
#include "Secrets.h"
#include <sqlite3.h>

extern FoodManager foodManager;
extern DailyNutrition dailyTotals;
extern FoodItem currentFood;
extern bool timeSynced;
extern bool needDisplayUpdate;
extern float weight;
extern DisplayManager displayManager;
extern float lastGrams;
extern String lastTimestamp;
extern String lastMode; // optional, if used in display



WiFiModeType WebServerManager::getCurrentMode() {
  return currentMode;
}

IPAddress WebServerManager::getDeviceIP() {
  return (currentMode == MODE_STA) ? WiFi.localIP() : WiFi.softAPIP();
}


void WebServerManager::begin(const char* ssid, const char* password, sqlite3* database) {
    db = database;
server.on("/foods", HTTP_GET, [this]() {
    if (!checkAuth()) return;

    String json = "[";
    const char* query = R"(
        SELECT Food.name, IFNULL(ColorMap.color_name, '') AS color
        FROM Food LEFT JOIN ColorMap ON Food.food_id = ColorMap.food_id;
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) == SQLITE_OK) {
        bool first = true;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (!first) json += ",";
            first = false;

            String name  = String(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
            String color = String(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));

            json += "{";
            json += "\"name\":\"" + name + "\",";
            json += "\"color\":\"" + color + "\"";
            json += "}";
        }
        sqlite3_finalize(stmt);
    }

    json += "]";
    server.send(200, "application/json", json);
});


    startWiFi(ssid, password);
    server.on("/", HTTP_GET, [this]() {
      if (!checkAuth()) return;
      handleRoot();
  });
  server.on("/addfood", HTTP_GET, [this]() {
    if (!checkAuth()) return;
    handleAddFood();
});
server.on("/deletefood", HTTP_GET, [this]() {
    if (!checkAuth()) return;
    if (!server.hasArg("name")) {
        server.send(400, "text/plain", "❌ Missing 'name'");
        return;
    }

    String foodName = server.arg("name");

    // Build SQL statements
    const char* deleteColorSQL = R"(
        DELETE FROM ColorMap
        WHERE food_id = (SELECT food_id FROM Food WHERE name = ?);
    )";

    const char* deleteFoodSQL = R"(
        DELETE FROM Food WHERE name = ?;
    )";

    // Delete ColorMap entry
    sqlite3_stmt* stmt1;
    if (sqlite3_prepare_v2(db, deleteColorSQL, -1, &stmt1, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt1, 1, foodName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt1);  // Ignore result
        sqlite3_finalize(stmt1);
    }

    // Delete Food entry
    sqlite3_stmt* stmt2;
    bool deleted = false;
    if (sqlite3_prepare_v2(db, deleteFoodSQL, -1, &stmt2, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt2, 1, foodName.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt2) == SQLITE_DONE && sqlite3_changes(db) > 0) {
            deleted = true;
        }
        sqlite3_finalize(stmt2);
    }

    if (deleted) {
        server.send(200, "text/plain", "✅ Deleted " + foodName);
    } else {
        server.send(404, "text/plain", "❌ Food not found: " + foodName);
    }
});
server.on("/select", HTTP_GET, [this]() {
  if (!checkAuth()) return;
  handleSelect();
});
server.on("/reset", HTTP_GET, [this]() {
  if (!checkAuth()) return;
  handleReset();
});
server.on("/daily", HTTP_GET, [this]() {
    String json = "{";
    json += "\"calories\":" + String(dailyTotals.calories) + ",";
    json += "\"protein\":" + String(dailyTotals.protein) + ",";
    json += "\"carbs\":" + String(dailyTotals.carbs) + ",";
    json += "\"fat\":" + String(dailyTotals.fat);
    json += "}";
    server.send(200, "application/json", json);
});

    server.on("/manifest.json", HTTP_GET, [this]() {
      File f = SD.open("/manifest.json");
      if (f) {
          server.streamFile(f, "application/json");
          f.close();
      } else {
          server.send(404, "text/plain", "manifest.json not found");
      }
  });
  
  server.on("/service-worker.js", HTTP_GET, [this]() {
      File f = SD.open("/service-worker.js");
      if (f) {
          server.streamFile(f, "application/javascript");
          f.close();
      } else {
          server.send(404, "text/plain", "service-worker.js not found");
      }
  });
  
  server.on("/icon-192.png", HTTP_GET, [this]() {
      File f = SD.open("/icon-192.png");
      if (f) {
          server.streamFile(f, "image/png");
          f.close();
      } else {
          server.send(404, "text/plain", "icon-192.png not found");
      }
  });
  
  server.on("/icon-512.png", HTTP_GET, [this]() {
      File f = SD.open("/icon-512.png");
      if (f) {
          server.streamFile(f, "image/png");
          f.close();
      } else {
          server.send(404, "text/plain", "icon-512.png not found");
      }
  });
  
  

    server.begin();
    Serial.println("🌐 HTTP Server started");
}

void WebServerManager::handle() {
    server.handleClient();
}

bool WebServerManager::checkAuth() {
  if (server.authenticate(AUTH_USER, AUTH_PASS)) {
      return true;
  }
  server.requestAuthentication();  // Basic Auth
  return false;
}



void WebServerManager::startWiFi(const char* ssid, const char* password) {
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");

  unsigned long startAttemptTime = millis();
  bool connected = false;

  while (millis() - startAttemptTime < 8000) {
      if (WiFi.status() == WL_CONNECTED) {
          connected = true;
          break;
      }
      delay(500);
      Serial.print(".");
  }
  Serial.println();

  if (connected) {
      Serial.print("✅ Connected! IP: ");
      Serial.println(WiFi.localIP());
      currentMode = MODE_STA;  // ✅ must be in scope
      syncTime();
  } else {
      Serial.println("⚠️ Failed to connect. Starting AP mode...");
      WiFi.setTxPower(WIFI_POWER_19_5dBm); // Max power
WiFi.softAP("KitchenScale", "12345678", 6, 0, 1);  // Good channel, 1 client
      Serial.print("📡 AP IP: ");
      Serial.println(WiFi.softAPIP());
      currentMode = MODE_AP;  // ✅ must be in scope
  }
}



void WebServerManager::syncTime() {
    configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org");
    Serial.println("🔄 NTP time sync requested.");
}

void WebServerManager::logSpectrumEntry(const String& foodName, float grams, const String& color, const Spectrum& spectrum) {
    const char* fileName = "/spectrum_log.csv";

    // Create CSV if needed
    if (!SD.exists(fileName)) {
        File init = SD.open(fileName, FILE_WRITE);
        if (init) {
            init.print("Food,Grams,Color");
            for (int i = 0; i < 10; ++i) init.printf(",Spec%d", i);
            init.println();
            init.close();
        }
    }

    // Append entry
    File file = SD.open(fileName, FILE_APPEND);
    if (file) {
        file.print(foodName); file.print(",");
        file.print(grams, 1); file.print(",");
        file.print(color);
        for (int i = 0; i < 10; ++i) {
            file.print(","); file.print(spectrum[i]);
        }
        file.println();
        file.close();
        Serial.println("📁 Spectrum entry logged.");
    } else {
        Serial.println("⚠️ Failed to open /spectrum_log.csv");
    }
}

String FoodManager::getColorForFood(const String& foodName) {
    auto it = foodColorMap.find(foodName);
    return (it != foodColorMap.end()) ? it->second : "";
}


void WebServerManager::handleSelect() {
    if (!server.hasArg("food") || !server.hasArg("grams") || !server.hasArg("color")) {
        server.send(400, "text/plain", "Missing parameters");
        return;
    }

    String foodName = server.arg("food");
    float grams = server.arg("grams").toFloat();
    String colorName = server.arg("color");

    if (grams <= 0) {
        server.send(400, "text/plain", "Invalid grams");
        return;
    }

    sqlite3* db = foodManager.getDatabaseHandle();
    const char* query = "SELECT food_id, calories, protein, carbs, fat FROM Food WHERE name = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        server.send(500, "text/plain", "DB query error");
        return;
    }

    sqlite3_bind_text(stmt, 1, foodName.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int food_id      = sqlite3_column_int(stmt, 0);
        float kcal       = sqlite3_column_double(stmt, 1);
        float prot       = sqlite3_column_double(stmt, 2);
        float carbs      = sqlite3_column_double(stmt, 3);
        float fat        = sqlite3_column_double(stmt, 4);
        sqlite3_finalize(stmt);

        float factor = grams / 100.0;
        float cal = kcal * factor;
        float p   = prot * factor;
        float c   = carbs * factor;
        float f   = fat * factor;

        // Update in-memory daily totals
        dailyTotals.calories += cal;
        dailyTotals.protein  += p;
        dailyTotals.carbs    += c;
        dailyTotals.fat      += f;

        // Timestamp
        String timestamp = "offline";
        if (timeSynced && WiFi.status() == WL_CONNECTED) {
            time_t now = time(nullptr);
            struct tm tm;
            localtime_r(&now, &tm);
            char buf[25];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
            timestamp = buf;
        }

        // Insert into LogEntry
        const char* insertSQL = "INSERT INTO LogEntry (timestamp, food_id, grams, calories, protein, carbs, fat) VALUES (?, ?, ?, ?, ?, ?, ?)";
        sqlite3_stmt* logStmt;
        if (sqlite3_prepare_v2(db, insertSQL, -1, &logStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(logStmt, 1, timestamp.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(logStmt, 2, food_id);
            sqlite3_bind_double(logStmt, 3, grams);
            sqlite3_bind_double(logStmt, 4, cal);
            sqlite3_bind_double(logStmt, 5, p);
            sqlite3_bind_double(logStmt, 6, c);
            sqlite3_bind_double(logStmt, 7, f);
            sqlite3_step(logStmt);
            sqlite3_finalize(logStmt);
        }

        // Update ColorMap
        const char* colorSQL = "REPLACE INTO ColorMap (food_id, color_name) VALUES (?, ?)";
        sqlite3_stmt* colorStmt;
        if (sqlite3_prepare_v2(db, colorSQL, -1, &colorStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(colorStmt, 1, food_id);
            sqlite3_bind_text(colorStmt, 2, colorName.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(colorStmt);
            sqlite3_finalize(colorStmt);
        }

        // Update Display
currentFood.name     = foodName;
currentFood.calories = kcal;
currentFood.protein  = prot;
currentFood.carbs    = carbs;
currentFood.fat      = fat;

lastGrams = grams;          // 🟡 Save for display
lastTimestamp = timestamp;  // 🟡 Save for display

displayManager.updateDisplay(grams, &currentFood, dailyTotals, timestamp, colorName);




        server.send(200, "text/plain", "✅ Logged " + String(grams) + "g of " + foodName);
        needDisplayUpdate = true;

    } else {
        sqlite3_finalize(stmt);
        server.send(404, "text/plain", "❌ Food not found");
    }
}


void WebServerManager::handleRoot() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'>
  <title>Smart Kitchen Scale</title>
  <script src='https://cdn.jsdelivr.net/npm/chart.js'></script>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f8ff; }
    h1 { color: #333; }
    select, input[type='text'], input[type='number'], button {
      padding: 12px; margin-bottom: 10px; font-size: 18px; border-radius: 5px; border: none;
    }
    button { background-color: #4CAF50; color: white; }
    button:hover { background-color: #45a049; }
    pre { background-color: #e0f7fa; padding: 10px; border-radius: 5px; }
    #toast {
      visibility: hidden; min-width: 250px; background-color: #4CAF50; color: white;
      text-align: center; border-radius: 5px; padding: 16px; position: fixed;
      z-index: 1; left: 50%; bottom: 30px; transform: translateX(-50%); font-size: 17px;
    }
    #toast.show { visibility: visible; animation: fadein 0.5s, fadeout 0.5s 2.5s; }
    @keyframes fadein { from { bottom: 0; opacity: 0; } to { bottom: 30px; opacity: 1; } }
    @keyframes fadeout { from { bottom: 30px; opacity: 1; } to { bottom: 0; opacity: 0; } }
    .controls { display: flex; flex-wrap: wrap; gap: 10px; align-items: center; margin-bottom: 20px; }
    .controls select, .controls input { flex: 1; min-width: 150px; }
    .daily-container { display: flex; flex-wrap: wrap; align-items: center; gap: 20px; margin-top: 20px; }
  </style>
</head>
<body>
  <h1>Smart Kitchen Scale</h1>
  <h2>Current Weight: <span id='liveWeight'>0g</span></h2>
  <button id="tareBtn">Tare Scale</button>

  <div class='controls'>
    <select id='sortSelect' onchange='sortFoods()'>
      <option value='newest'>Newest Added</option>
      <option value='oldest'>Oldest Added</option>
      <option value='most'>Most Selected</option>
      <option value='least'>Least Selected</option>
    </select>
    <input type='text' id='searchInput' oninput='sortFoods()' placeholder='Search food.'>
  </div>

  <div id='foodList'></div>

  <h2>Add New Food</h2>
  <input type='text' id='newName' placeholder='Name (e.g. Apple)'>
  <input type='number' id='newProtein' placeholder='Protein per 100g' oninput='calculateCalories()'>
  <input type='number' id='newCarbs' placeholder='Carbs per 100g' oninput='calculateCalories()'>
  <input type='number' id='newFat' placeholder='Fat per 100g' oninput='calculateCalories()'>
  <input type='number' id='newCalories' placeholder='Calories per 100g' readonly>
  <button onclick='submitNewFood()'>Add New Food</button>

  <h2>Daily Totals</h2>
  <div class='daily-container'>
    <div id='dailyTotals'>Loading...</div>
    <canvas id='macroChart' width='250' height='250'></canvas>
  </div>

  <h2>Reset Daily Totals</h2>
  <button onclick='resetTotals()'>Reset Totals</button>

  <h2>Status:</h2><pre id='status'>Select, add, or delete a food</pre>
  <div id='toast'></div>

  <script>
    var foods = [];
    var ws, macroChart;

    window.addEventListener("load", () => {
      document.getElementById('tareBtn').onclick = () => ws.send("tare");
    });

    function startWebSocket() {
      ws = new WebSocket('ws://' + location.hostname + ':81');
      ws.onmessage = function(event) {
        document.getElementById('liveWeight').innerText = event.data + 'g';
      };
      ws.onclose = function() { setTimeout(startWebSocket, 2000); };
    }

    function fetchFoods() {
      fetch('/foods')
        .then(r => r.json())
        .then(data => {
          foods = data.map((f, i) => Object.assign(f, {usage: 0, order: i}));
          sortFoods();
        });
    }

    function sortFoods() {
      var mode = document.getElementById('sortSelect').value;
      if (mode == 'newest') foods.sort((a,b)=>b.order-a.order);
      if (mode == 'oldest') foods.sort((a,b)=>a.order-b.order);
      if (mode == 'most') foods.sort((a,b)=>b.usage-a.usage);
      if (mode == 'least') foods.sort((a,b)=>a.usage-b.usage);
      showFoods();
    }

    function showFoods() {
      var container = document.getElementById('foodList');
      var query = document.getElementById('searchInput').value.toLowerCase();
      container.innerHTML = '';
      foods.forEach(function(f, idx) {
        if (f.name.toLowerCase().includes(query)) {
          var wrapper = document.createElement('div');
          wrapper.style.display = 'flex';
          wrapper.style.alignItems = 'center';
          wrapper.style.marginBottom = '10px';

          var btn = document.createElement('button');
          btn.className = 'foodButton';
          btn.innerText = f.name;
          btn.style.flex = '1';
          btn.onclick = function() { logFood(f); };

          var del = document.createElement('button');
          del.innerText = '❌';
          del.style.backgroundColor = '#f44336';
          del.style.marginLeft = '10px';
          del.onclick = function() { confirmDelete(f.name, idx); };

          wrapper.appendChild(btn);
          wrapper.appendChild(del);
          container.appendChild(wrapper);
        }
      });
    }

    function logFood(foodObj) {
      let grams = prompt('How many grams of ' + foodObj.name + '?');
      if (!grams || isNaN(grams) || grams <= 0) return;

      let color = foodObj.color || prompt('What color is the food (e.g. red, green)?');
      if (!color) return;

      fetch(`/select?food=${encodeURIComponent(foodObj.name)}&grams=${grams}&color=${encodeURIComponent(color.toLowerCase())}`)
        .then(r => r.text())
        .then(t => {
          document.getElementById('status').innerText = t;
          updateDailyTotals();
          showToast('✅ Food logged!');
        })
        .catch(_ => {
          document.getElementById('status').innerText = 'Error logging food';
          showToast('❌ Error');
        });
    }

    function updateDailyTotals() {
      fetch('/daily')
        .then(r => r.json())
        .then(data => {
          var div = document.getElementById('dailyTotals');
          div.innerHTML = 'Calories: ' + data.calories.toFixed(0) + ' kcal<br>' +
                          'Protein: ' + data.protein.toFixed(0) + ' g<br>' +
                          'Carbs: ' + data.carbs.toFixed(0) + ' g<br>' +
                          'Fat: ' + data.fat.toFixed(0) + ' g';
          var { protein, carbs, fat } = data;
          if (protein === 0 && carbs === 0 && fat === 0) {
            macroChart.data.datasets[0].data = [1];
            macroChart.data.datasets[0].backgroundColor = ['#cccccc'];
            macroChart.data.labels = ['Empty'];
          } else {
            macroChart.data.datasets[0].data = [protein, carbs, fat];
            macroChart.data.datasets[0].backgroundColor = ['#4CAF50', '#2196F3', '#FFC107'];
            macroChart.data.labels = ['Protein', 'Carbs', 'Fat'];
          }
          macroChart.update();
        });
    }

    function resetTotals() {
      fetch('/reset')
        .then(r => r.text())
        .then(t => {
          document.getElementById('status').innerText = t;
          updateDailyTotals();
          showToast('✅ Totals reset!');
        });
    }

    function confirmDelete(name, index) {
      if (confirm('Are you sure you want to delete ' + name + '?')) {
        fetch('/deletefood?name=' + encodeURIComponent(name))
          .then(r => r.text())
          .then(t => {
            document.getElementById('status').innerText = t;
            foods.splice(index, 1);
            showFoods();
            showToast('✅ Deleted');
          });
      }
    }

    function submitNewFood() {
      let n = document.getElementById('newName').value.trim(),
          p = document.getElementById('newProtein').value.trim(),
          c = document.getElementById('newCarbs').value.trim(),
          f = document.getElementById('newFat').value.trim(),
          cal = document.getElementById('newCalories').value.trim();
      if (n && p && c && f && cal) {
        let q = '/addfood?name=' + encodeURIComponent(n) + '&protein=' + p + '&carbs=' + c + '&fat=' + f + '&calories=' + cal;
        fetch(q).then(r => r.text()).then(t => {
          document.getElementById('status').innerText = t;
          fetchFoods();
          clearNewFoodForm();
          showToast('✅ Added');
        });
      } else {
        showToast('⚠️ Fill all fields');
      }
    }

    function calculateCalories() {
      var p = parseFloat(document.getElementById('newProtein').value) || 0,
          c = parseFloat(document.getElementById('newCarbs').value) || 0,
          f = parseFloat(document.getElementById('newFat').value) || 0;
      document.getElementById('newCalories').value = (p*4 + c*4 + f*9).toFixed(1);
    }

    function clearNewFoodForm() {
      ['newName','newProtein','newCarbs','newFat','newCalories'].forEach(id => document.getElementById(id).value = '');
    }

    function showToast(msg) {
      var t = document.getElementById('toast');
      t.innerText = msg;
      t.className = 'show';
      setTimeout(() => { t.className = ''; }, 3000);
    }

    window.onload = function() {
      startWebSocket();
      fetchFoods();
      updateDailyTotals();
      var ctx = document.getElementById('macroChart').getContext('2d');
      macroChart = new Chart(ctx, {
        type: 'pie',
        data: { labels: [], datasets: [{ data: [], backgroundColor: [] }] },
        options: { responsive: false, plugins: { legend: { position: 'bottom' } } }
      });
    };
  </script>
</body>
</html>
)rawliteral";

    server.send(200, "text/html", html);
}



void WebServerManager::handleReset() {
    dailyTotals = {0, 0, 0, 0};
    currentFood.name = "";
currentFood.calories = 0;
currentFood.protein = 0;
currentFood.carbs = 0;
currentFood.fat = 0;
    needDisplayUpdate = true;
    server.send(200, "text/plain", "✅ Daily totals reset");
}

void WebServerManager::handleAddFood() {
    if (!server.hasArg("name") || !server.hasArg("calories") ||
        !server.hasArg("protein") || !server.hasArg("carbs") || !server.hasArg("fat")) {
        server.send(400, "text/plain", "Missing parameters");
        return;
    }

    foodManager.addFood(
        server.arg("name"),
        server.arg("calories").toFloat(),
        server.arg("protein").toFloat(),
        server.arg("carbs").toFloat(),
        server.arg("fat").toFloat()
    );

    server.send(200, "text/plain", "✅ Food added: " + server.arg("name"));
}

void WebServerManager::handleDeleteFood() {
    if (!server.hasArg("name")) {
        server.send(400, "text/plain", "Missing food name");
        return;
    }

    bool success = foodManager.deleteFood(server.arg("name"));
    if (success) {
        server.send(200, "text/plain", "✅ Food deleted");
    } else {
        server.send(404, "text/plain", "❌ Food not found");
    }
}
