#include "DisplayManager.h"
#include <WiFi.h>  // For access to WiFi status and IP
#include "WebServerManager.h"

void DisplayManager::begin() {
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.println("Smart Kitchen Scale");
}

void DisplayManager::updateDisplay(float weight, FoodItem* currentFood, DailyNutrition& totals, const String& ip, const String& mode) {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
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
                   currentFood->carbs * fct,
                   currentFood->fat   * fct);
    } else {
        tft.println("No selection\n");
    }

    tft.println("Daily Totals:");
    tft.printf("Cal: %.0f  Prot: %.0f\n", totals.calories, totals.protein);
    tft.printf("Carb: %.0f  Fat:  %.0f\n", totals.carbs, totals.fat);

    // Draw IP at the bottom in small font, if connected
    if (!ip.isEmpty()) {
        tft.setTextSize(1);
        tft.setCursor(0, tft.height() - 20);
        tft.print("Mode: ");
        tft.print(mode);
    
        tft.setCursor(0, tft.height() - 10);
        tft.print("IP: ");
        tft.print(ip);
    }
        
}
