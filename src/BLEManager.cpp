#include "BLEManager.h"

// BLE UUIDs
#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_CHAR_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_CHAR_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

extern std::vector<FoodItem> foodDatabase;
extern DailyNutrition dailyTotals;
extern FoodItem* currentFood;
extern float weight;
extern bool needDisplayUpdate;

static BLEManager* bleManagerRef = nullptr;  // Static reference!

class ServerCallbacks : public BLEServerCallbacks {
    void onDisconnect(BLEServer* pServer) override {
        Serial.println("🔌 BLE client disconnected, restarting advertising");
        pServer->startAdvertising();
    }
};

class RxCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) override {
        if (bleManagerRef) {
            BLECommand cmd;
            size_t len = pCharacteristic->getLength();
            if (len > sizeof(cmd.data)) len = sizeof(cmd.data);
            memcpy(cmd.data, pCharacteristic->getData(), len);
            cmd.len = len;
            BaseType_t woken = pdFALSE;
            xQueueSendFromISR(bleManagerRef->getQueue(), &cmd, &woken);
            if (woken) portYIELD_FROM_ISR();
        }
    }
};

void BLEManager::begin() {
    BLEDevice::init("KitchenScaleBLE");

    BLEServer* pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    BLEService* pService = pServer->createService(NUS_SERVICE_UUID);

    pRxCharacteristic = pService->createCharacteristic(
        NUS_CHAR_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new RxCallbacks());

    pTxCharacteristic = pService->createCharacteristic(
        NUS_CHAR_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    pService->start();
    pServer->getAdvertising()->start();

    Serial.println("🔵 BLE advertising ‘KitchenScaleBLE’");

    bleQueue = xQueueCreate(10, sizeof(BLECommand));
    bleManagerRef = this;  // Set static pointer to this
}

void BLEManager::loop() {
    processBLE();
}

void BLEManager::processBLE() {
    BLECommand cmd;
    while (xQueueReceive(bleQueue, &cmd, 0) == pdTRUE) {
        String s;
        s.reserve(cmd.len);
        for (int i = 0; i < cmd.len; ++i) s += (char)cmd.data[i];
        s.trim();

        for (auto& item : foodDatabase) {
            if (s.equalsIgnoreCase(item.name)) {
                currentFood = &item;
                float factor = weight / 100.0f;
                dailyTotals.calories += item.calories * factor;
                dailyTotals.protein  += item.protein  * factor;
                dailyTotals.carbs    += item.carbs    * factor;
                dailyTotals.fat      += item.fat      * factor;
                needDisplayUpdate = true;

                String response = "Logged: " + item.name;
                sendNotification(response);
                break;
            }
        }
    }
}

void BLEManager::sendNotification(const String& message) {
    if (pTxCharacteristic) {
        pTxCharacteristic->setValue(message.c_str());
        pTxCharacteristic->notify();
    }
}
