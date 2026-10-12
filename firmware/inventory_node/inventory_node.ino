#include <Arduino.h>
#include <DHT.h>
#include "wire.h"
#ifndef DHT_PIN
#define DHT_PIN 4
#endif
#ifndef PIR_PIN
#define PIR_PIN 27
#endif
#ifndef DHT_TYPE
#define DHT_TYPE DHT11
#endif
#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN 2
#endif
DHT sensor(DHT_PIN, DHT_TYPE);
uint32_t sequence = 0;
uint32_t lastSample = 0;
uint32_t transition = 0;
bool candidate = false;
bool presence = false;
float filteredTemperature = 0;
float filteredHumidity = 0;
bool initialized = false;
void setup() {
    Serial.begin(115200);
    pinMode(PIR_PIN, INPUT);
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
    sensor.begin();
}
void loop() {
    const uint32_t now = millis();
    const bool rawPresence = digitalRead(PIR_PIN) == HIGH;
    if (rawPresence != candidate) { candidate = rawPresence; transition = now; }
    if ((uint32_t)(now - transition) >= 150) presence = candidate;
    digitalWrite(STATUS_LED_PIN, presence ? HIGH : LOW);
    if ((uint32_t)(now - lastSample) < 2000) { delay(1); return; }
    lastSample = now;
    const float temperature = sensor.readTemperature();
    const float humidity = sensor.readHumidity();
    const bool valid = isfinite(temperature) && isfinite(humidity) && temperature >= -40 && temperature <= 80 && humidity >= 0 && humidity <= 100;
    if (valid) {
        if (!initialized) { filteredTemperature = temperature; filteredHumidity = humidity; initialized = true; }
        else { filteredTemperature += 0.25f * (temperature - filteredTemperature); filteredHumidity += 0.25f * (humidity - filteredHumidity); }
    }
    const wire_sample sample{sequence++, now, (int16_t)(valid ? lroundf(filteredTemperature * 100) : 0), (uint16_t)(valid ? lroundf(filteredHumidity * 100) : 0), (uint8_t)presence, (uint8_t)valid};
    uint8_t frame[WIRE_FRAME_SIZE]; wire_encode(&sample, frame); Serial.write(frame, sizeof frame);
}
