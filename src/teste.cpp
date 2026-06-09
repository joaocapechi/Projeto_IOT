#include <Arduino.h>

#include "esp_heap_caps.h"

void setup() {
    Serial.begin(115200);

    if(psramFound()) {
        Serial.println("PSRAM OK");
        Serial.printf(
            "PSRAM size: %u\n",
            ESP.getPsramSize()
        );
    }
    else {
        Serial.println("NO PSRAM");
    }
}

unsigned long long tempo = 0;

void loop() {
    if (millis() - tempo >= 1000) {
        if(psramFound()) {
            Serial.println("PSRAM OK");
            Serial.printf(
                "PSRAM size: %u\n",
                ESP.getPsramSize()
            );
        }
        else {
            Serial.println("NO PSRAM");
        }
        tempo = millis();
    }
}