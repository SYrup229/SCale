#include "WebServerManager.h"
#include "DisplayManager.h"
#include "WebSocketManager.h"
#include "ColorMap.h" 
#include <SD.h>
#include "Secrets.h"

extern FoodManager foodManager;
extern DailyNutrition dailyTotals;
extern FoodItem* currentFood;
extern bool timeSynced;
extern bool needDisplayUpdate;
extern float weight;

void WebServerManager::begin(const char* ssid, const char* password) {
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
  handleDeleteFood();
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
  if (!checkAuth()) return;
        String json = "{";
        json += "\"calories\":" + String(dailyTotals.calories, 2) + ",";
        json += "\"protein\":" + String(dailyTotals.protein, 2) + ",";
        json += "\"carbs\":" + String(dailyTotals.carbs, 2) + ",";
        json += "\"fat\":" + String(dailyTotals.fat, 2);
        json += "}";
        server.send(200, "application/json", json);
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
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi");
    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("✅ Connected! IP: ");
        Serial.println(WiFi.localIP());
        syncTime();
    } else {
        Serial.println("⚠️ Failed to connect. Proceeding offline.");
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


void WebServerManager::handleSelect() {

    if (!server.hasArg("food") || !server.hasArg("grams")) {
        server.send(400, "text/plain", "Missing required parameters (food, grams)");
        return;
    }
    
    String foodName = server.arg("food");
    float weightGrams = server.arg("grams").toFloat();
    
    if (weightGrams <= 0) {
        server.send(400, "text/plain", "Invalid grams value");
        return;
    }
    
    // ✅ Declare colorName AFTER foodName is known
    String colorName;
    if (server.hasArg("color")) {
        colorName = server.arg("color");
    } else if (foodManager.foodColorMap.count(foodName)) {
        colorName = foodManager.foodColorMap[foodName];
        Serial.printf("🎨 Using remembered color for %s: %s\n", foodName.c_str(), colorName.c_str());
    } else {
        server.send(400, "text/plain", "Missing color and no known color for this food");
        return;
    }
    

    weight = weightGrams;

    Spectrum spectrum = {};
    bool colorFound = false;

    if (colorSpectra.count(colorName.c_str())) {
        spectrum = colorSpectra.at(colorName.c_str());
        colorFound = true;
        Serial.printf("🎨 Simulated spectrum loaded for color: %s\n", colorName.c_str());
    } else {
        Serial.printf("⚠️ Unknown color '%s'. Spectrum will be empty.\n", colorName.c_str());
    }

    for (auto& item : foodManager.getDatabase()) {
        if (foodName.equalsIgnoreCase(item.name)) {
            currentFood = &item;

            float factor = weight / 100.0;
            float cal   = item.calories * factor;
            float prot  = item.protein  * factor;
            float carbs = item.carbs    * factor;
            float fat   = item.fat      * factor;

            dailyTotals.calories += cal;
            dailyTotals.protein  += prot;
            dailyTotals.carbs    += carbs;
            dailyTotals.fat      += fat;

            needDisplayUpdate = true;

            // ✅ Log entries
            logFoodEntry(item.name, weight, cal, prot, carbs, fat);
            logSpectrumEntry(item.name, weight, colorName, spectrum);

            // ✅ Save color mapping
            foodManager.foodColorMap[item.name] = colorName;
            foodManager.saveColorMapToSD();

            // ✅ Recalculate usage count (for live sorting)
            foodManager.analyzeFoodLog();

            String resp = "✅ Logged " + String(weight, 0) + "g of " + item.name;
            if (!colorFound) resp += " (⚠️ Unknown color, spectrum empty)";
            server.send(200, "text/plain", resp);
            return;
        }
    }

    server.send(404, "text/plain", "❌ Food not found");
}


void WebServerManager::logFoodEntry(const String& foodName, float grams, float cal, float prot, float carbs, float fat) {
    Serial.println("🔍 Entered logFoodEntry()");
    Serial.printf("Food = %s | %.1fg | %.2f cal, %.2fP %.2fC %.2fF\n",
                  foodName.c_str(), grams, cal, prot, carbs, fat);

    File f = SD.open("/food_log.csv", FILE_APPEND);
    if (!f) {
        Serial.println("❌ Failed to open food_log.csv");
        return;
    }

    String timestamp = "offline";
    if (timeSynced && WiFi.status() == WL_CONNECTED) {
        time_t now = time(nullptr);
        struct tm tm;
        localtime_r(&now, &tm);
        char buf[25];
strftime(buf, sizeof(buf), "%Y_%m_%d_%H_%M_%S", &tm);
        timestamp = buf;
    }

    // ✅ Quote the timestamp so spreadsheet programs treat it as plain text
    f.printf("\"%s\",%s,%.1f,%.2f,%.2f,%.2f,%.2f\n",
             timestamp.c_str(), foodName.c_str(), grams, cal, prot, carbs, fat);

    f.close();
    Serial.println("✅ Logged food entry to food_log.csv");
}




void WebServerManager::handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                "<title>Smart Kitchen Scale</title>"
                "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>"
                "<style>"
                "body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f8ff; }"
                "h1 { color: #333; }"
                "select, input[type='text'], input[type='number'], button { padding: 12px; margin-bottom: 10px; font-size: 18px; border-radius: 5px; border: none; }"
                "button { background-color: #4CAF50; color: white; }"
                "button:hover { background-color: #45a049; }"
                "pre { background-color: #e0f7fa; padding: 10px; border-radius: 5px; }"
                "#toast { visibility: hidden; min-width: 250px; background-color: #4CAF50; color: white; text-align: center; border-radius: 5px; padding: 16px; position: fixed; z-index: 1; left: 50%; bottom: 30px; transform: translateX(-50%); font-size: 17px; }"
                "#toast.show { visibility: visible; animation: fadein 0.5s, fadeout 0.5s 2.5s; }"
                "@keyframes fadein { from { bottom: 0; opacity: 0; } to { bottom: 30px; opacity: 1; } }"
                "@keyframes fadeout { from { bottom: 30px; opacity: 1; } to { bottom: 0; opacity: 0; } }"
                ".controls { display: flex; flex-wrap: wrap; gap: 10px; align-items: center; margin-bottom: 20px; }"
                ".controls select, .controls input { flex: 1; min-width: 150px; }"
                "@media (min-width: 768px) { select, input[type='text'], input[type='number'], button { width: auto; } }"
                ".daily-container { display: flex; flex-wrap: wrap; align-items: center; gap: 20px; margin-top: 20px; }"
                "</style>";

  // Generate JS array of food objects with color
  html += "<script>var foods = [";
  for (size_t i = 0; i < foodManager.getDatabase().size(); i++) {
      const auto& f = foodManager.getDatabase()[i];
      String color = foodManager.foodColorMap.count(f.name) ? foodManager.foodColorMap[f.name] : "";
      html += "{name:'" + f.name + "',usage:" + String(f.usageCount) +
              ",order:" + String(f.addedOrder) + ",color:'" + color + "'}";
      if (i != foodManager.getDatabase().size() - 1) html += ",";
  }
  html += "];";

  html += R"rawliteral(
    var ws;
    var macroChart;

    function startWebSocket() {
      ws = new WebSocket('ws://' + location.hostname + ':81');
      ws.onmessage = function(event) {
        document.getElementById('liveWeight').innerText = event.data + 'g';
      };
      ws.onclose = function() { setTimeout(startWebSocket, 2000); };
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

    function sortFoods() {
      var mode = document.getElementById('sortSelect').value;
      if (mode == 'newest') foods.sort((a,b)=>b.order-a.order);
      if (mode == 'oldest') foods.sort((a,b)=>a.order-b.order);
      if (mode == 'most') foods.sort((a,b)=>b.usage-a.usage);
      if (mode == 'least') foods.sort((a,b)=>a.usage-b.usage);
      showFoods();
    }

    function logFood(foodObj) {
      let grams = prompt('How many grams of ' + foodObj.name + '?');
      if (!grams || isNaN(grams) || grams <= 0) return;

      let color = foodObj.color;
      if (!color) {
        color = prompt('What color is the food (e.g. red, green)?');
        if (!color) return;
      }

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
          var protein = data.protein;
          var carbs = data.carbs;
          var fat = data.fat;
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
          })
          .catch(_ => {
            document.getElementById('status').innerText = 'Error deleting';
            showToast('❌ Error deleting');
          });
      }
    }

    function submitNewFood() {
      let n=document.getElementById('newName').value.trim(), p=document.getElementById('newProtein').value.trim(), c=document.getElementById('newCarbs').value.trim(), f=document.getElementById('newFat').value.trim(), cal=document.getElementById('newCalories').value.trim();
      if(n&&p&&c&&f&&cal){
        let q='/addfood?name='+encodeURIComponent(n)+'&protein='+p+'&carbs='+c+'&fat='+f+'&calories='+cal;
        fetch(q).then(r=>r.text()).then(t=>{
          document.getElementById('status').innerText=t;
          foods.push({name:n,usage:0,order:foods.length,color:''});
          sortFoods();
          clearNewFoodForm();
          showToast('✅ Added');
        });
      }else{
        showToast('⚠️ Fill all fields');
      }
    }

    function calculateCalories() {
      var p=parseFloat(document.getElementById('newProtein').value)||0, c=parseFloat(document.getElementById('newCarbs').value)||0, f=parseFloat(document.getElementById('newFat').value)||0;
      document.getElementById('newCalories').value = (p*4+c*4+f*9).toFixed(1);
    }

    function clearNewFoodForm() {
      ['newName','newProtein','newCarbs','newFat','newCalories'].forEach(id=>document.getElementById(id).value='');
    }

    function showToast(msg) {
      var t = document.getElementById('toast'); t.innerText=msg; t.className='show'; setTimeout(()=>{t.className='';},3000);
    }

    window.onload=function(){
      startWebSocket();
      sortFoods();
      updateDailyTotals();
      var ctx=document.getElementById('macroChart').getContext('2d');
      macroChart=new Chart(ctx,{type:'pie',data:{labels:[],datasets:[{data:[],backgroundColor:[]}]},options:{responsive:false,plugins:{legend:{position:'bottom'}}}});
    };
  )rawliteral";

  html += "</script>";

  html += "<body><h1>Smart Kitchen Scale</h1>"
          "<h2>Current Weight: <span id='liveWeight'>0g</span></h2>"
          "<div class='controls'>"
          "<select id='sortSelect' onchange='sortFoods()'>"
          "<option value='newest'>Newest Added</option>"
          "<option value='oldest'>Oldest Added</option>"
          "<option value='most'>Most Selected</option>"
          "<option value='least'>Least Selected</option>"
          "</select>"
          "<input type='text' id='searchInput' oninput='sortFoods()' placeholder='Search food...'>"
          "</div>"
          "<div id='foodList'></div>"

          "<h2>Add New Food</h2>"
          "<input type='text' id='newName' placeholder='Name (e.g. Apple)'>"
          "<input type='number' id='newProtein' placeholder='Protein per 100g' oninput='calculateCalories()'>"
          "<input type='number' id='newCarbs' placeholder='Carbs per 100g' oninput='calculateCalories()'>"
          "<input type='number' id='newFat' placeholder='Fat per 100g' oninput='calculateCalories()'>"
          "<input type='number' id='newCalories' placeholder='Calories per 100g' readonly>"
          "<button onclick='submitNewFood()'>Add New Food</button>"

          "<h2>Daily Totals</h2>"
          "<div class='daily-container'>"
          "<div id='dailyTotals'>Loading...</div>"
          "<canvas id='macroChart' width='250' height='250'></canvas>"
          "</div>"

          "<h2>Reset Daily Totals</h2>"
          "<button onclick='resetTotals()'>Reset Totals</button>"

          "<h2>Status:</h2><pre id='status'>Select, add, or delete a food</pre>"
          "<div id='toast'></div></body></html>";

  server.send(200, "text/html", html);
}





void WebServerManager::handleReset() {
    dailyTotals = {0, 0, 0, 0};
    currentFood = nullptr;
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
