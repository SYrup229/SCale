#include "WebSocketManager.h"
#include "Scale_LoadCell.h"
extern float weight;
extern bool tareScale;

void WebSocketManager::begin() {
    webSocket.begin();
    webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
        if (type == WStype_CONNECTED) {
            Serial.printf("[WebSocket] Client %u Connected\n", num);
        } else if (type == WStype_DISCONNECTED) {
            Serial.printf("[WebSocket] Client %u Disconnected\n", num);
        } else if (type == WStype_TEXT) {
            String msg((char*)payload, length);
            if (msg == "tare") {
                tareScale = true;
                Serial.println("🟡 Tare command received");
            }
        }
    });
    Serial.println("🌐 WebSocket Server started on port 81");
}



void WebSocketManager::handle(float weight, bool& tareScale) {
    webSocket.loop();

    // Broadcast weight every second
    if (millis() - lastSendTime > 250) {
        lastSendTime = millis();
        String payload = String(weight, 0);
        webSocket.broadcastTXT(payload);
    }

    // Handle tare scale command
    if (tareScale) {

        scale_tare();
        tareScale = false;
    }
}
