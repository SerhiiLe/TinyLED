#include <Arduino.h>
#include <FastLED.h>

#if defined(ESP32C3)
  #define PIN_LED 6
#elif defined(ESP32)
  #define PIN_LED 22
#else // ESP8266
  #define PIN_LED 5
#endif

#define NUM_LEDS 64

CRGB leds[NUM_LEDS];
uint8_t gHue = 0;

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812B, PIN_LED, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(80);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 1000);
    FastLED.clear();
    FastLED.show();
}

void loop() {
    // Fill rainbow effect
    fill_rainbow(leds, NUM_LEDS, gHue, 7);

    // Fade trail effect
    fadeToBlackBy(leds, NUM_LEDS, 20);
    int pos = beatsin16(13, 0, NUM_LEDS - 1);
    leds[pos] += CHSV(gHue, 255, 255);

    // Palette pulsing
    CRGBPalette16 palette = PartyColors_p;
    uint8_t beat = beatsin8(62, 64, 255);
    leds[0] = ColorFromPalette(palette, gHue, beat);

    FastLED.show();

    EVERY_N_MILLISECONDS(20) {
        gHue++;
    }
}

