#include "esp_mac.h"
#pragma once
#include <Arduino.h>
#include <WebSocketsServer.h>

class WebSocketManager {
public:
    void begin();
    void handle(float weight);

private:
    WebSocketsServer webSocket = WebSocketsServer(81);
    unsigned long lastSendTime = 0;
};
