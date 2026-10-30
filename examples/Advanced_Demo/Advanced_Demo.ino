/*
  TinyLED - Advanced Demo
  
  Demonstrates advanced capabilities of TinyLED:
  - 2D Serpentine Matrix mapping (32x8 = 256 LEDs)
  - FastLED drop-in API with dual-buffer crossfading
  - 6 animated pattern effects:
      1. 2D Plasma wave (xyMatrix)
      2. Rainbow with Glitter
      3. Confetti with fade trail
      4. Sinelon (beating dots with fade trail)
      5. Juggle (8 multi-phase colored dots)
      6. BPM (palette pulsing with PartyColors)
  - Current limiter (setMaxPowerInVoltsAndMilliamps) & Color correction
  - Automatic pattern cycling every 10 seconds
  - Works out-of-the-box on ESP8266, ESP32, ESP32-C3, ESP32-S2/S3, and RP2040 (Raspberry Pi Pico)
*/

#include <Arduino.h>
#include <TinyLED.h>

// Automatically select appropriate default pin for each platform
#ifdef PIN_LED
  #undef PIN_LED
#endif
#if defined(ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C3)
  #define PIN_LED 6
#elif defined(ESP32)
  #define PIN_LED 22
#elif defined(ARDUINO_ARCH_RP2040) || defined(PICO_RP2040) || defined(TARGET_RP2040) || defined(RP2040)
  #define PIN_LED 16
#else // ESP8266
  #define PIN_LED 5
#endif

// Matrix dimensions (32 x 8 = 256 LEDs)
#define WIDTH 32
#define HEIGHT 8
#define SEGMENTS 1
#define NUM_LEDS (WIDTH * HEIGHT * SEGMENTS)

#define DEFAULT_BRIGHTNESS 50
#define DEFAULT_POWER 2000 // 2000 mA (2A @ 5V)

// Primary and secondary framebuffers (for transition effects)
CRGB leds[NUM_LEDS];
CRGB ledsNext[NUM_LEDS];

// Optional template instance showing TinyLED standalone syntax
TinyLED<PIN_LED, NUM_LEDS, ORDER_GRB> strip;

uint8_t led_brightness = 0;
uint8_t gHue = 0;
uint8_t gCurrentPatternNumber = 0;

// ============================================================================
// 2D Matrix Coordinate Mapping (Serpentine / Zig-zag)
// ============================================================================

uint16_t getPixelNumber(int8_t x, int8_t y) {
    if (y % 2 == 0) {
        return y * WIDTH + x;
    } else {
        return y * WIDTH + (WIDTH - x - 1);
    }
}

void drawPixelXY(int8_t x, int8_t y, CRGB color, CRGB arr[] = nullptr) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    int thisPixel = getPixelNumber(x, y) * SEGMENTS;
    for (int i = 0; i < SEGMENTS; i++) {
        if (arr)
            arr[thisPixel + i] = color;
        else
            leds[thisPixel + i] = color;
    }
}

uint32_t getPixColor(int num, CRGB arr[] = nullptr) {
    int thisPixel = num * SEGMENTS;
    if (thisPixel < 0 || thisPixel >= NUM_LEDS) return 0;
    if (arr)
        return (((uint32_t)arr[thisPixel].r << 16) | ((uint32_t)arr[thisPixel].g << 8) | (uint32_t)arr[thisPixel].b);
    else
        return (((uint32_t)leds[thisPixel].r << 16) | ((uint32_t)leds[thisPixel].g << 8) | (uint32_t)leds[thisPixel].b);
}

uint32_t getPixColorXY(int8_t x, int8_t y, CRGB arr[] = nullptr) {
    return getPixColor(getPixelNumber(x, y) * SEGMENTS, arr);
}

// ============================================================================
// Dual-buffer Transition & Helper Functions
// ============================================================================

void swapBuffers() {
    for (int i = 0; i < NUM_LEDS; i++) {
        CRGB tmp = leds[i];
        leds[i] = ledsNext[i];
        ledsNext[i] = tmp;
    }
}

void saveBuffer() {
    memcpy(ledsNext, leds, sizeof(ledsNext));
}

bool crossFade(bool init = false) {
    static int8_t step = 0;
    if (init) {
        swapBuffers();
        step = 9;
    }
    if (!step) return false;
    if (step-- == 1) {
        swapBuffers();
        return true;
    }
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = blend(leds[i], ledsNext[i], 80);
    }
    return true;
}

void set_brightness(uint8_t br) {
    if (br != led_brightness) {
        FastLED.setBrightness(br);
        led_brightness = br;
    }
}

void display_setup() {
    FastLED.addLeds<WS2812B, PIN_LED, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    set_brightness(DEFAULT_BRIGHTNESS);
    if (DEFAULT_POWER > 0) FastLED.setMaxPowerInVoltsAndMilliamps(5, DEFAULT_POWER);
    FastLED.clear();
    saveBuffer();

    // Visual self-test: 3 corner pixels (Red, Blue, Green)
    drawPixelXY(0, 0, CRGB::Red);
    drawPixelXY(0, HEIGHT - 1, CRGB::Blue);
    drawPixelXY(WIDTH - 1, 0, CRGB::Green);

    FastLED.show();
}

// ============================================================================
// Animated Demo Effects
// ============================================================================

// 1. Rainbow
void rainbow() {
    fill_rainbow(leds, NUM_LEDS, gHue, 7);
}

// 2. Rainbow with random white sparkles
void addGlitter(fract8 chanceOfGlitter) {
    if (random8() < chanceOfGlitter) {
        leds[random16(NUM_LEDS)] += CRGB::White;
    }
}

void rainbowWithGlitter() {
    rainbow();
    addGlitter(80);
}

// 3. Confetti: random colored speckles that blink and fade smoothly
void confetti() {
    fadeToBlackBy(leds, NUM_LEDS, 10);
    int pos = random16(NUM_LEDS);
    leds[pos] += CHSV(gHue + random8(64), 200, 255);
}

// 4. Sinelon: a colored dot sweeping back and forth with fading trails
void sinelon() {
    fadeToBlackBy(leds, NUM_LEDS, 20);
    int pos = beatsin16(13, 0, NUM_LEDS - 1);
    leds[pos] += CHSV(gHue, 255, 192);
}

// 5. BPM: colored stripes pulsing at a defined Beats-Per-Minute
void bpm() {
    uint8_t BeatsPerMinute = 62;
    CRGBPalette16 palette = PartyColors_p;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
    }
}

// 6. Juggle: eight colored dots, weaving in and out of sync with each other
void juggle() {
    fadeToBlackBy(leds, NUM_LEDS, 20);
    uint8_t dothue = 0;
    for (int i = 0; i < 8; i++) {
        leds[beatsin16(i + 7, 0, NUM_LEDS - 1)] |= CHSV(dothue, 200, 255);
        dothue += 32;
    }
}

// 7. 2D Plasma wave on matrix
void DrawOneFrame(uint8_t startHue8, int8_t yHueDelta8, int8_t xHueDelta8) {
    uint8_t lineStartHue = startHue8;
    for (uint8_t y = 0; y < HEIGHT; y++) {
        lineStartHue += yHueDelta8;
        uint8_t pixelHue = lineStartHue;
        for (uint8_t x = 0; x < WIDTH; x++) {
            pixelHue += xHueDelta8;
            drawPixelXY(x, y, CHSV(pixelHue, 255, 255));
        }
    }
}

void xyMatrix() {
    uint32_t ms = millis();
    int32_t yHueDelta32 = ((int32_t)cos16(ms * 27) * (350 / WIDTH));
    int32_t xHueDelta32 = ((int32_t)cos16(ms * 39) * (310 / HEIGHT));
    DrawOneFrame((uint8_t)(ms / 65536), (int8_t)(yHueDelta32 / 32768), (int8_t)(xHueDelta32 / 32768));
}

// Pattern playlist
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = { xyMatrix, rainbowWithGlitter, confetti, sinelon, juggle, bpm };
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void nextPattern() {
    gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE(gPatterns);
}

void demo_tick() {
    gPatterns[gCurrentPatternNumber]();
    FastLED.show();
    gHue++;
    EVERY_N_SECONDS(10) {
        nextPattern();
    }
}

// ============================================================================
// Setup and Loop
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("\n=== TinyLED Advanced Demo Starting ==="));

    // 1. Brief self-test of TinyLED template class API (green flash)
    Serial.println(F("Testing TinyLED template class API (green flash)..."));
    strip.setCorrection(Correction::TypicalLEDStrip);
    strip.setMaxPower(5, DEFAULT_POWER);
    strip.setBrightness(DEFAULT_BRIGHTNESS);
    fill_solid(strip.leds(), NUM_LEDS, CRGB::Green);
    strip.show();
    delay(300);
    strip.clear(true);
    delay(100);

    // 2. Initialize FastLED display compatibility mode
    Serial.println(F("Initializing FastLED compatibility mode..."));
    display_setup();
    delay(500);

    Serial.println(F("Initialization complete, running demo patterns!"));
}

void loop() {
    demo_tick();

    EVERY_N_SECONDS(2) {
        Serial.print(F("Pattern: "));
        Serial.print(gCurrentPatternNumber);
        Serial.print(F(", Hue: "));
        Serial.print(gHue);
        Serial.print(F(", Heap: "));
#if defined(ESP8266) || defined(ESP32)
        Serial.println(ESP.getFreeHeap());
#elif defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
        Serial.println(rp2040.getFreeHeap());
#else
        Serial.println(F("N/A"));
#endif
    }

    delay(15);
}

