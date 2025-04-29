#include "WebServerManager.h"
#include "DisplayManager.h"
#include "WebSocketManager.h"

extern FoodManager foodManager;
extern DailyNutrition dailyTotals;
extern FoodItem* currentFood;
extern bool timeSynced;
extern bool needDisplayUpdate;
extern float weight;

void WebServerManager::begin(const char* ssid, const char* password) {
    startWiFi(ssid, password);
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/addfood", HTTP_GET, [this]() { handleAddFood(); });
    server.on("/deletefood", HTTP_GET, [this]() { handleDeleteFood(); });
    server.on("/select", HTTP_GET, [this]() { handleSelect(); });
    server.on("/reset", HTTP_GET, [this]() { handleReset(); });
    server.on("/daily", HTTP_GET, [this]() {
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
    Serial.println("🔄 NTP time sync requested...");
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
  
    html += "<script>var foods = [";
  
    for (size_t i = 0; i < foodManager.getDatabase().size(); i++) {
      const auto& f = foodManager.getDatabase()[i];
      html += "{name:'" + f.name + "',usage:" + String(f.usageCount) + ",order:" + String(f.addedOrder) + "}";
      if (i != foodManager.getDatabase().size() - 1) html += ",";
    }
  
    html += "];"
            "var ws;"
            "var macroChart;"
            "function startWebSocket() {"
            "  ws = new WebSocket('ws://' + location.hostname + ':81');"
            "  ws.onmessage = function(event) {"
            "    document.getElementById('liveWeight').innerText = event.data + 'g';"
            "  };"
            "  ws.onclose = function() { setTimeout(startWebSocket, 2000); };"
            "}"
            "function showFoods() {"
            "  var container = document.getElementById('foodList');"
            "  var query = document.getElementById('searchInput').value.toLowerCase();"
            "  container.innerHTML = '';"
            "  foods.forEach(function(f, idx) {"
            "    if (f.name.toLowerCase().includes(query)) {"
            "      var wrapper = document.createElement('div');"
            "      wrapper.style.display = 'flex';"
            "      wrapper.style.alignItems = 'center';"
            "      wrapper.style.marginBottom = '10px';"
            "      var btn = document.createElement('button');"
            "      btn.className = 'foodButton';"
            "      btn.innerText = f.name;"
            "      btn.style.flex = '1';"
            "      btn.onclick = function() { logFood(f.name); };"
            "      var del = document.createElement('button');"
            "      del.innerText = '❌';"
            "      del.style.backgroundColor = '#f44336';"
            "      del.style.marginLeft = '10px';"
            "      del.onclick = function() { confirmDelete(f.name, idx); };"
            "      wrapper.appendChild(btn);"
            "      wrapper.appendChild(del);"
            "      container.appendChild(wrapper);"
            "    }"
            "  });"
            "}"
            "function sortFoods() {"
            "  var mode = document.getElementById('sortSelect').value;"
            "  if (mode == 'newest') foods.sort((a,b)=>b.order-a.order);"
            "  if (mode == 'oldest') foods.sort((a,b)=>a.order-b.order);"
            "  if (mode == 'most') foods.sort((a,b)=>b.usage-a.usage);"
            "  if (mode == 'least') foods.sort((a,b)=>a.usage-b.usage);"
            "  showFoods();"
            "}"
            "function logFood(food) {"
            "  fetch('/select?food=' + encodeURIComponent(food))"
            "    .then(response => response.text())"
            "    .then(text => {"
            "      document.getElementById('status').innerText = text;"
            "      updateDailyTotals();"
            "      showToast('✅ Food logged!');"
            "    })"
            "    .catch(err => {"
            "      document.getElementById('status').innerText = 'Error logging food';"
            "      showToast('❌ Error logging food');"
            "    });"
            "}"
            "function updateDailyTotals() {"
            "  fetch('/daily')"
            "    .then(response => response.json())"
            "    .then(data => {"
            "      var div = document.getElementById('dailyTotals');"
            "      div.innerHTML = 'Calories: ' + data.calories.toFixed(0) + ' kcal<br>' +"
            "                     'Protein: ' + data.protein.toFixed(0) + ' g<br>' +"
            "                     'Carbs: ' + data.carbs.toFixed(0) + ' g<br>' +"
            "                     'Fat: ' + data.fat.toFixed(0) + ' g';"
            "      var protein = data.protein;"
            "      var carbs = data.carbs;"
            "      var fat = data.fat;"
            "      if (protein === 0 && carbs === 0 && fat === 0) {"
            "        macroChart.data.datasets[0].data = [1];"
            "        macroChart.data.datasets[0].backgroundColor = ['#cccccc'];"
            "        macroChart.data.labels = ['Empty'];"
            "      } else {"
            "        macroChart.data.datasets[0].data = [protein, carbs, fat];"
            "        macroChart.data.datasets[0].backgroundColor = ['#4CAF50', '#2196F3', '#FFC107'];"
            "        macroChart.data.labels = ['Protein', 'Carbs', 'Fat'];"
            "      }"
            "      macroChart.update();"
            "    });"
            "}"
            "function resetTotals() {"
            "  fetch('/reset')"
            "    .then(response => response.text())"
            "    .then(text => {"
            "      document.getElementById('status').innerText=text;"
            "      showToast('✅ Totals reset!');"
            "      updateDailyTotals();"
            "    })"
            "    .catch(err => {"
            "      document.getElementById('status').innerText='Error resetting totals';"
            "      showToast('❌ Error resetting totals');"
            "    });"
            "}"
            "function confirmDelete(name, index) {"
            "  if (confirm('Are you sure you want to delete \"' + name + '\"?')) {"
            "    fetch('/deletefood?name=' + encodeURIComponent(name))"
            "      .then(response => response.text())"
            "      .then(text => {"
            "        document.getElementById('status').innerText = text;"
            "        foods.splice(index, 1);"
            "        showFoods();"
            "        showToast('✅ Food \"' + name + '\" deleted!');"
            "      })"
            "      .catch(err => {"
            "        document.getElementById('status').innerText = 'Error deleting food';"
            "        showToast('❌ Error deleting food');"
            "      });"
            "  }"
            "}"
            "function submitNewFood() {"
            "  var name = document.getElementById('newName').value.trim();"
            "  var calories = document.getElementById('newCalories').value.trim();"
            "  var protein = document.getElementById('newProtein').value.trim();"
            "  var carbs = document.getElementById('newCarbs').value.trim();"
            "  var fat = document.getElementById('newFat').value.trim();"
            "  if (name && calories && protein && carbs && fat) {"
            "    var query = '/addfood?name=' + encodeURIComponent(name) +"
            "                '&calories=' + encodeURIComponent(calories) +"
            "                '&protein=' + encodeURIComponent(protein) +"
            "                '&carbs=' + encodeURIComponent(carbs) +"
            "                '&fat=' + encodeURIComponent(fat);"
            "    fetch(query)"
            "      .then(response => response.text())"
            "      .then(text => {"
            "        document.getElementById('status').innerText = text;"
            "        foods.push({name:name, usage:0, order:foods.length});"
            "        sortFoods();"
            "        clearNewFoodForm();"
            "        showToast('✅ Food \"' + name + '\" added!');"
            "      })"
            "      .catch(err => {"
            "        document.getElementById('status').innerText = 'Error adding food';"
            "        showToast('❌ Error adding food');"
            "      });"
            "  } else {"
            "    showToast('⚠️ Fill all fields');"
            "  }"
            "}"
            "function calculateCalories() {"
            "  var protein=parseFloat(document.getElementById('newProtein').value)||0;"
            "  var carbs=parseFloat(document.getElementById('newCarbs').value)||0;"
            "  var fat=parseFloat(document.getElementById('newFat').value)||0;"
            "  var calories=(protein*4)+(carbs*4)+(fat*9);"
            "  document.getElementById('newCalories').value=calories.toFixed(1);"
            "}"
            "function clearNewFoodForm() {"
            "  document.getElementById('newName').value='';"
            "  document.getElementById('newProtein').value='';"
            "  document.getElementById('newCarbs').value='';"
            "  document.getElementById('newFat').value='';"
            "  document.getElementById('newCalories').value='';"
            "}"
            "function showToast(message) {"
            "  var toast = document.getElementById('toast');"
            "  toast.innerText=message;"
            "  toast.className='show';"
            "  setTimeout(function(){ toast.className = toast.className.replace('show',''); },3000);"
            "}"
            "window.onload=function(){"
            "  startWebSocket();"
            "  sortFoods();"
            "  updateDailyTotals();"
            "  var ctx = document.getElementById('macroChart').getContext('2d');"
            "  macroChart = new Chart(ctx, {"
            "    type: 'pie',"
            "    data: { labels: [], datasets: [{ data: [], backgroundColor: [] }] },"
            "    options: { responsive: false, plugins: { legend: { position: 'bottom' } } }"
            "  });"
            "};"
            "</script>";
  
    html += "</head><body>"
            "<h1>Smart Kitchen Scale</h1>"
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
            "<div id='toast'></div>"
            "</body></html>";
  
    server.send(200, "text/html", html);
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

void WebServerManager::handleSelect() {
    if (!server.hasArg("food")) {
        server.send(400, "text/plain", "Missing food parameter");
        return;
    }

    String selectedFood = server.arg("food");
    for (auto& item : foodManager.getDatabase()) {
        if (selectedFood.equalsIgnoreCase(item.name)) {
            currentFood = &item;
            float factor = weight / 100.0;
            dailyTotals.calories += item.calories * factor;
            dailyTotals.protein  += item.protein  * factor;
            dailyTotals.carbs    += item.carbs    * factor;
            dailyTotals.fat      += item.fat      * factor;
            needDisplayUpdate = true;
            server.send(200, "text/plain", "✅ Logged: " + item.name);
            return;
        }
    }

    server.send(404, "text/plain", "❌ Food not found");
}

void WebServerManager::handleReset() {
    dailyTotals = {0, 0, 0, 0};
    currentFood = nullptr;
    needDisplayUpdate = true;
    server.send(200, "text/plain", "✅ Daily totals reset");
}
