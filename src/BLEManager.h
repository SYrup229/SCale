#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "FoodManager.h"

// Make BLECommand PUBLIC
struct BLECommand {
    uint8_t data[64];
    uint8_t len;
};

class BLEManager {
public:
QueueHandle_t getQueue() {
    return bleQueue;
}

    void begin();
    void sendNotification(const String& message);
    void loop();  // New: call loop inside main

private:
    void processBLE();

    BLECharacteristic* pTxCharacteristic;
    BLECharacteristic* pRxCharacteristic;
    QueueHandle_t bleQueue = nullptr;
};
