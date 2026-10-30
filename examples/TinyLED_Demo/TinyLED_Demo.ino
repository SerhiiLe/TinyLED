#include <Arduino.h>
#include <TinyLED.h>

#if defined(ESP32C3)
  #define PIN_LED 6
#elif defined(ESP32)
  #define PIN_LED 22
#else // ESP8266
  #define PIN_LED 5
#endif

#define NUM_LEDS 32

TinyLED<PIN_LED, NUM_LEDS, ORDER_GRB> strip;
uint8_t gHue = 0;

void setup() {
    Serial.begin(115200);
    strip.setCorrection(Correction::TypicalLEDStrip);
    strip.setMaxPower(5, 1000);
    strip.setBrightness(100);
    strip.clear();
    strip.show();
}

void loop() {
    strip[0] = CRGB::Red;
    strip[1] = CHSV(gHue, 255, 255);
    strip[2] = blend(strip[0], strip[1], 128);

    strip.show();

    EVERY_N_MILLISECONDS(25) {
        gHue++;
    }
}

