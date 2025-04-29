#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <time.h>
#include <WebSocketsServer.h>  // For WebSocket server


// ─── CONFIG ───────────────────────────────────────────────────────────────────
// Wi-Fi credentials
const char* ssid     = "Mihaita";
const char* password = "ciolan229";

// SD card CS pin
const int SD_CS = 10;

// Filenames
const char* foodDBFileName = "/food_db.csv";
const char* logFileName    = "/food_log.csv";

// Simulated weight in **grams**
float weight = 1000.0;  // 1000 g = 1 kg

// BLE NUS UUIDs
#define NUS_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_CHAR_UUID_RX        "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_CHAR_UUID_TX        "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// ─── DATA STRUCTURES ───────────────────────────────────────────────────────────
struct FoodItem {
  String name;
  float calories, protein, carbs, fat;
  int   addedOrder;  // Order of addition
  int   usageCount;  // How many times logged
};

struct DailyNutrition {
  float calories, protein, carbs, fat;
};

// In-memory DB + state
std::vector<FoodItem> foodDatabase;
DailyNutrition       dailyTotals       = {0,0,0,0};
FoodItem*            currentFood       = nullptr;
bool                 needDisplayUpdate = true;
bool                 timeSynced        = false;

// BLE command packet
struct BLECommand {
  uint8_t  data[64];
  uint8_t  len;
};
static QueueHandle_t bleQueue = nullptr;

// ─── HARDWARE OBJECTS ─────────────────────────────────────────────────────────
TFT_eSPI            tft;
WebSocketsServer webSocket = WebSocketsServer(81); // WebSocket server on port 81
WebServer           server(80);
BLECharacteristic*  pTxCharacteristic;
BLECharacteristic*  pRxCharacteristic;

// ─── PROTOTYPES ────────────────────────────────────────────────────────────────
void loadFoodDatabase();
void resetDailyTotals();
String processSelection(const String& foodName);
void handleRoot();
void handleSelect();
void logMeasurement();
void updateDisplay();
void analyzeFoodLog();
void BLEProcessTask(void*);

// ─── BLE CALLBACKS ─────────────────────────────────────────────────────────────
class ServerCallbacks : public BLEServerCallbacks {
  void onDisconnect(BLEServer* srv) override {
    Serial.println("🔌 BLE client disconnected, restarting advertising");
    srv->startAdvertising();
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* chr) override {
    BLECommand cmd;
    size_t len = chr->getLength();
    if (len > sizeof(cmd.data)) len = sizeof(cmd.data);
    memcpy(cmd.data, chr->getData(), len);
    cmd.len = len;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(bleQueue, &cmd, &woken);
    if (woken) portYIELD_FROM_ISR();
  }
};

// ─── SETUP ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);

  // — SD CARD —
  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD init failed");
  } else {
    Serial.println("✅ SD initialized");
    loadFoodDatabase();
    analyzeFoodLog();
    if (!SD.exists(logFileName)) {
      File f = SD.open(logFileName, FILE_WRITE);
      if (f) {
        f.println("DateTime,Weight(g),Food,Calories,Protein,Carbs,Fat");
        f.close();
      }
    }
  }

  // — TFT DISPLAY —
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.println("Smart Kitchen Scale");

  // — WIFI —
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("✅ Wi-Fi IP = ");
    Serial.println(WiFi.localIP());

    configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org");
    Serial.println("🔄 NTP time sync requested...");
  } else {
    Serial.println("⚠️ Wi-Fi timeout, offline mode");
  }

  // — BLE NUS setup —
  BLEDevice::init("KitchenScaleBLE");
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  BLEService* nus = pServer->createService(NUS_SERVICE_UUID);

  pRxCharacteristic = nus->createCharacteristic(
    NUS_CHAR_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_WRITE_NR
  );
  pRxCharacteristic->setCallbacks(new RxCallbacks());

  pTxCharacteristic = nus->createCharacteristic(
    NUS_CHAR_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxCharacteristic->addDescriptor(new BLE2902());

  nus->start();
  pServer->getAdvertising()->start();
  Serial.println("🔵 BLE advertising ‘KitchenScaleBLE’");

  webSocket.begin();
webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_CONNECTED) {
    Serial.printf("[WebSocket] Client %u Connected\n", num);
  } else if (type == WStype_DISCONNECTED) {
    Serial.printf("[WebSocket] Client %u Disconnected\n", num);
  }
});
Serial.println("🌐 WebSocket server started");


  // — HTTP server routes —
  server.on("/",       HTTP_GET, handleRoot);
  server.on("/reset", HTTP_GET, []() {
    resetDailyTotals();
    server.send(200, "text/plain", "✅ Totals Reset");
  });  
  server.on("/addfood", HTTP_GET, []() {
    if (!server.hasArg("name") || !server.hasArg("calories") ||
        !server.hasArg("protein") || !server.hasArg("carbs") || !server.hasArg("fat")) {
      server.send(400, "text/plain", "Missing parameters");
      return;
    }
    server.on("/deletefood", HTTP_GET, []() {
      if (!server.hasArg("name")) {
        server.send(400, "text/plain", "Missing food name");
        return;
      }
      String target = server.arg("name");
      std::vector<FoodItem> newDB;
      for (auto& item : foodDatabase) {
        if (!item.name.equalsIgnoreCase(target)) {
          newDB.push_back(item);
        }
      }
      if (newDB.size() == foodDatabase.size()) {
        server.send(404, "text/plain", "Food not found");
        return;
      }
      // Overwrite food_db.csv
      File f = SD.open(foodDBFileName, FILE_WRITE);
      if (!f) {
        server.send(500, "text/plain", "Failed to update food database");
        return;
      }
      f.println("Name,Calories,Protein,Carbs,Fat");
      for (auto& item : newDB) {
        f.println(item.name + "," +
                  String(item.calories, 2) + "," +
                  String(item.protein, 2) + "," +
                  String(item.carbs, 2) + "," +
                  String(item.fat, 2));
      }
      f.close();
      foodDatabase = newDB;
      server.send(200, "text/plain", "✅ Food deleted");
    });
    
  
    String name     = server.arg("name");
    String calories = server.arg("calories");
    String protein  = server.arg("protein");
    String carbs    = server.arg("carbs");
    String fat      = server.arg("fat");
  
    // Append to food_db.csv
    File f = SD.open(foodDBFileName, FILE_APPEND);
    if (!f) {
      server.send(500, "text/plain", "Failed to open food_db.csv");
      return;
    }
  
    f.println(name + "," + calories + "," + protein + "," + carbs + "," + fat);
    f.close();
    
    // Optional: Add to RAM database immediately
    foodDatabase.push_back({
      name,
      calories.toFloat(),
      protein.toFloat(),
      carbs.toFloat(),
      fat.toFloat()
    });
  
    server.send(200, "text/plain", "✅ New food added: " + name);
  });
  
  server.on("/select", HTTP_GET, handleSelect);
  server.begin();
  Serial.println("🌐 HTTP server started");

  // — BLE processing queue & task on core 1 —
  bleQueue = xQueueCreate(10, sizeof(BLECommand));
  xTaskCreatePinnedToCore(
    BLEProcessTask,
    "BLEProcessTask",
    4096,
    nullptr,
    2,
    nullptr,
    1
  );

  resetDailyTotals();
}

// ─── MAIN LOOP ────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  webSocket.loop();

// Send weight update over WebSocket every 1 sec
static unsigned long lastSend = 0;
if (millis() - lastSend > 1000) {
  lastSend = millis();
  String payload = String(weight, 0);
  webSocket.broadcastTXT(payload);
}


  // Non-blocking time sync detection
  if (!timeSynced && time(nullptr) > 24 * 3600) { // > Jan 2, 1970
    Serial.println("✅ Time synchronized!");
    timeSynced = true;
  }

  if (needDisplayUpdate) {
    updateDisplay();
    needDisplayUpdate = false;
  }
  delay(50);
}

// ─── BLE PROCESS TASK ─────────────────────────────────────────────────────────
void BLEProcessTask(void*) {
  BLECommand cmd;
  for (;;) {
    if (xQueueReceive(bleQueue, &cmd, portMAX_DELAY) == pdTRUE) {
      String s; s.reserve(cmd.len);
      for (int i = 0; i < cmd.len; i++) s += char(cmd.data[i]);
      s.trim();
      String resp = processSelection(s);
      pTxCharacteristic->setValue(resp.c_str());
      pTxCharacteristic->notify();
    }
  }
}

// ─── IMPLEMENTATIONS ───────────────────────────────────────────────────────────
void loadFoodDatabase() {
  if (!SD.exists(foodDBFileName)) {
    Serial.println("⚠️ food_db.csv missing");
    return;
  }
  File f = SD.open(foodDBFileName, FILE_READ);
  if (!f) {
    Serial.println("⚠️ Cannot open food_db.csv");
    return;
  }
  bool first = true;
  int order = 0;
  foodDatabase.clear();
  Serial.println("Loading DB...");
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) continue;
    if (first && line.startsWith("Name")) { first = false; continue; }
    first = false;
    std::vector<String> cols;
    int start = 0, idx;
    for (int i = 0; i < 4; ++i) {
      idx = line.indexOf(',', start);
      if (idx == -1) break;
      cols.push_back(line.substring(start, idx));
      start = idx + 1;
    }
    cols.push_back(line.substring(start));
    if (cols.size() < 5) {
      Serial.println("⚠️ Skipping malformed line: " + line);
      continue;
    }
    foodDatabase.push_back({
      cols[0],
      cols[1].toFloat(),
      cols[2].toFloat(),
      cols[3].toFloat(),
      cols[4].toFloat(),
      order++,   // addedOrder
      0          // usageCount (will fill later)
    });
    Serial.println(" • " + cols[0]);
  }
  f.close();
  Serial.printf("✅ %u items loaded\n", (unsigned)foodDatabase.size());
}

void analyzeFoodLog() {
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
    std::vector<String> cols;
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
  Serial.println("✅ Usage counts analyzed");
}


void resetDailyTotals() {
  dailyTotals = {0,0,0,0};
  currentFood = nullptr;
  needDisplayUpdate = true;
}

// Shared logic for HTTP & BLE
String processSelection(const String& q) {
  for (auto& it : foodDatabase) {
    if (q.equalsIgnoreCase(it.name)) {
      currentFood = &it;
      logMeasurement();
      float factor = weight / 100.0;
      dailyTotals.calories += it.calories * factor;
      dailyTotals.protein  += it.protein  * factor;
      dailyTotals.carbs    += it.carbs    * factor;
      dailyTotals.fat      += it.fat      * factor;
      needDisplayUpdate = true;

      String r = "Logged: " + it.name + "\n\n"
               + "Daily Totals:\n"
               + "Calories: " + String(dailyTotals.calories,2) + "\n"
               + "Protein:  " + String(dailyTotals.protein,2)  + "\n"
               + "Carbs:    " + String(dailyTotals.carbs,2)    + "\n"
               + "Fat:      " + String(dailyTotals.fat,2);
      return r;
    }
  }
  return "❌ Not found: " + q;
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                "<title>Smart Kitchen Scale</title>"
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
                "</style>"
                "<script>var foods = [";

  // Inject food array dynamically
  for (size_t i = 0; i < foodDatabase.size(); i++) {
    const auto& f = foodDatabase[i];
    html += "{name:'" + f.name + "',usage:" + String(f.usageCount) + ",order:" + String(f.addedOrder) + "}";
    if (i != foodDatabase.size() - 1) html += ",";
  }

  html += "];"
          "var ws;"
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
          "    .then(text => { document.getElementById('status').innerText = text; showToast('✅ Food logged!'); })"
          "    .catch(err => { document.getElementById('status').innerText = 'Error logging food'; showToast('❌ Error logging food'); });"
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
          "      .catch(err => { document.getElementById('status').innerText = 'Error adding food'; showToast('❌ Error adding food'); });"
          "  } else { showToast('⚠️ Fill all fields'); }"
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
          "      .catch(err => { document.getElementById('status').innerText = 'Error deleting food'; showToast('❌ Error deleting food'); });"
          "  }"
          "}"
          "function clearNewFoodForm() {"
          "  document.getElementById('newName').value='';"
          "  document.getElementById('newProtein').value='';"
          "  document.getElementById('newCarbs').value='';"
          "  document.getElementById('newFat').value='';"
          "  document.getElementById('newCalories').value='';"
          "}"
          "function calculateCalories() {"
          "  var protein=parseFloat(document.getElementById('newProtein').value)||0;"
          "  var carbs=parseFloat(document.getElementById('newCarbs').value)||0;"
          "  var fat=parseFloat(document.getElementById('newFat').value)||0;"
          "  var calories=(protein*4)+(carbs*4)+(fat*9);"
          "  document.getElementById('newCalories').value=calories.toFixed(1);"
          "}"
          "function resetTotals() {"
          "  fetch('/reset')"
          "    .then(response => response.text())"
          "    .then(text => { document.getElementById('status').innerText=text; showToast('✅ Totals reset!'); })"
          "    .catch(err => { document.getElementById('status').innerText='Error resetting totals'; showToast('❌ Error resetting totals'); });"
          "}"
          "function showToast(message) {"
          "  var toast = document.getElementById('toast');"
          "  toast.innerText=message;"
          "  toast.className='show';"
          "  setTimeout(function(){ toast.className = toast.className.replace('show',''); },3000);"
          "}"
          "window.onload=function(){startWebSocket();sortFoods();};"
          "</script>"
          "</head><body>"
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
          "<h2>Reset Daily Totals</h2>"
          "<button onclick='resetTotals()'>Reset Totals</button>"
          "<h2>Status:</h2><pre id='status'>Select, add, or delete a food</pre>"
          "<div id='toast'></div>"
          "</body></html>";

  server.send(200, "text/html", html);
}



void handleSelect() {
  if (!server.hasArg("food")) {
    server.send(400, "text/plain", "Missing food parameter");
    return;
  }
  String resp = processSelection(server.arg("food"));
  server.send(resp.startsWith("❌") ? 404 : 200, "text/plain", resp);
}

void logMeasurement() {
  File f = SD.open(logFileName, FILE_APPEND);
  if (!f) return;

  String stamp;
  if (timeSynced && WiFi.status() == WL_CONNECTED) {
    time_t now = time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    stamp = buf;
  } else {
    stamp = "offline mode";
  }

  float factor = weight / 100.0;
  String line = stamp + "," + String(weight,0) + ","
                + currentFood->name + ","
                + String(currentFood->calories * factor,2) + ","
                + String(currentFood->protein  * factor,2) + ","
                + String(currentFood->carbs    * factor,2) + ","
                + String(currentFood->fat      * factor,2);
  f.println(line);
  f.close();
  Serial.println("Logged: " + line);
}

void updateDisplay() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0);
  tft.setTextSize(2);
  tft.println("Kitchen Scale");
  tft.printf("Wt: %.0f g\n\n", weight);

  if (currentFood) {
    float fct = weight / 100.0;
    tft.println(currentFood->name);
    tft.printf("Cal: %.0f  Prot: %.0f\n",
               currentFood->calories * fct,
               currentFood->protein  * fct);
    tft.printf("Carb: %.0f  Fat:  %.0f\n\n",
               currentFood->carbs    * fct,
               currentFood->fat      * fct);
  } else {
    tft.println("No selection\n");
  }

  tft.println("Daily Totals:");
  tft.printf("Cal: %.0f  Prot: %.0f\n",
             dailyTotals.calories,
             dailyTotals.protein);
  tft.printf("Carb: %.0f  Fat:  %.0f\n",
             dailyTotals.carbs,
             dailyTotals.fat);
}
