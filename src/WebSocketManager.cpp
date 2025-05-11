#include "WebSocketManager.h"
extern float lastGrams;

void WebSocketManager::begin() {
    webSocket.begin();
    webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
        if (type == WStype_CONNECTED) {
            Serial.printf("[WebSocket] Client %u Connected\n", num);
        } else if (type == WStype_DISCONNECTED) {
            Serial.printf("[WebSocket] Client %u Disconnected\n", num);
        }
    });
    Serial.println("🌐 WebSocket Server started on port 81");
}



void WebSocketManager::handle(float lastGrams) {
    webSocket.loop();

    // Broadcast weight every second
    if (millis() - lastSendTime > 1000) {
        lastSendTime = millis();
        String payload = String(lastGrams,0);
        webSocket.broadcastTXT(payload);
    }
}
