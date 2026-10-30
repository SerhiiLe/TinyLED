# TinyLED

<p align="center">
  <a href="https://github.com/SerhiiLe/TinyLED/releases"><img src="https://img.shields.io/github/v/release/SerhiiLe/TinyLED?include_prereleases&color=blue" alt="Latest Release"></a>
  <a href="https://registry.platformio.org/libraries/serhiile/TinyLED"><img src="https://badges.registry.platformio.org/packages/serhiile/library/TinyLED.svg" alt="PlatformIO Registry"></a>
  <a href="https://www.arduino.cc/reference/en/libraries/tinyled/"><img src="https://img.shields.io/badge/Arduino-TinyLED-00979C.svg?logo=arduino" alt="Arduino Library"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-LGPL_v3-green.svg" alt="License: LGPL v3"></a>
</p>

<p align="center">
  <b>English</b> | <b><a href="README.md">Русский</a></b>
</p>

---

Ultra-lightweight, high-performance, and standalone C++ library for WS2812B (NeoPixel) addressable LEDs on:
* **ESP8266** (NodeMCU, Wemos D1 mini, etc.)
* **ESP32**, **ESP32-S2**, **ESP32-S3**, **ESP32-C3**, **ESP32-C6**, **ESP32-H2**
* **RP2040** (Raspberry Pi Pico / Pico W) and **RP2350** (Raspberry Pi Pico 2)

Designed as a **drop-in replacement** for FastLED in smart clock, scrolling text, information board, and LED matrix projects.

---

## Key Benefits

| Feature | FastLED 3.7+ | TinyLED |
|---|---|---|
| **Firmware Flash Size** | ~110 KB | **~2.2 KB** (> 105 KB saved!) |
| **Dynamic Memory (Heap)** | `malloc` / dynamic structures | **0 bytes** (zero dynamic allocation) |
| **ESP32, S2, S3, C3, C6, H2 Output** | Interrupt hooks / RMT driver | **Hardware RMT** (no Wi-Fi or FreeRTOS jitter) |
| **RP2040 (Pico) / RP2350 (Pico 2) Output** | Third-party ports / `pioasm` required | **Hardware PIO** (embedded microcode, any GPIO) |
| **ESP8266 Output** | Interrupts / frequent frame drops | **IRAM cycle-accurate bitbang** with timing guarantee |
| **Power Limiter** | Yes | **Yes** (WS2812B 5V model) |
| **Color Correction** | Yes | **Yes** (`TypicalLEDStrip` 0xFFB0F0) |
| **Operating Modes** | Controller only | **External buffer** (for 2+ buffers) or **Template class** (internal buffer) |
| **API Compatibility** | Original | **Full drop-in compatibility** with FastLED core API |

---

## Installation

### PlatformIO
Add the library to your `platformio.ini`:
```ini
[env:my_project]
framework = arduino
lib_deps =
    SerhiiLe/TinyLED
```
Or directly from the Git repository:
```ini
lib_deps =
    https://github.com/SerhiiLe/TinyLED.git
```

### Arduino IDE
1. **Via Library Manager:**
   * Go to *Sketch* $\rightarrow$ *Include Library* $\rightarrow$ *Manage Libraries...*
   * Type `TinyLED` into the search field and click **Install**.
2. **Manual Installation (ZIP Archive):**
   * Download the latest release from [Releases](https://github.com/SerhiiLe/TinyLED/releases).
   * Go to *Sketch* $\rightarrow$ *Include Library* $\rightarrow$ *Add .ZIP Library...*

---

## Example Sketches

The [`examples/`](examples/) folder contains ready-to-run examples:
1. **[TinyLED_Demo](examples/TinyLED_Demo/TinyLED_Demo.ino)** — Basic usage of the standalone `TinyLED<PIN, NUM_LEDS, ORDER>` template class with an internal buffer.
2. **[FastLED_Compatibility](examples/FastLED_Compatibility/FastLED_Compatibility.ino)** — FastLED drop-in replacement with an external pixel buffer, rainbow generator, and color palettes.
3. **[Advanced_Demo](examples/Advanced_Demo/Advanced_Demo.ino)** — Advanced full-feature demo sketch:
   * 32×8 serpentine 2D matrix coordinate mapping
   * Dual-buffer frame blending and crossfading (`crossFade` / `blend`)
   * 6 animated visual effects: 2D plasma wave (`xyMatrix`), rainbow with glitter (`rainbowWithGlitter`), confetti (`confetti`), sinelon sweeping dot (`sinelon`), juggle multi-phase dots (`juggle`), PartyColors palette beat (`bpm`)
   * Automatic pattern playlist cycling every 10 seconds and Serial diagnostics.

---

## Hardware Driver Architecture

### 1. ESP8266 (Xtensa lx106, 80 MHz / 160 MHz)
* Nanosecond-accurate pulse generation loop based on the `rsr.ccount` CPU cycle counter instruction.
* Driver code resides in **IRAM** (`IRAM_ATTR`), completely eliminating SPI Flash cache miss delays.
* Interrupts are disabled (`noInterrupts()`) strictly during transmission (~7.7 ms for 256 LEDs). The hardware watchdog timer is automatically refreshed (`ESP.wdtFeed()`) right before transmission.

### 2. ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2
* Non-blocking pulse generation using the dedicated hardware **RMT (Remote Control)** peripheral.
* Out-of-the-box compatibility with both ESP-IDF 4.x (Arduino Core 2.x) and ESP-IDF 5.x (Arduino Core 3.x using `<driver/rmt_tx.h>` and `<driver/rmt_encoder.h>`).
* Employs the **RMT Translator / Simple Encoder Callback**: byte-to-RMT pulse translation is done on-the-fly inside the RMT FIFO interrupt.
* **No intermediate RAM frame buffer needed**; zero CPU overhead during transmission.
* FreeRTOS and Wi-Fi network interrupts **remain enabled**, ensuring rock-solid stability for NTP, Telegram bots, and Web servers without LED flickering.

### 3. RP2040 / RP2350 (Raspberry Pi Pico / Pico W, Pico 2)
* Hardware generation driven by the integrated **PIO (Programmable I/O)** coprocessor.
* The 4-instruction WS2812 timing program is pre-assembled directly into the driver source code — **no external `pioasm` tool required**, compiling cleanly in any standard C++ toolchain (Arduino IDE & PlatformIO).
* Automatically claims an available State Machine on `pio0` or `pio1`.
* Clock divider is dynamically calculated from `clock_get_hz(clk_sys)`, guaranteeing accurate 800 kHz output across any system clock frequency (even when overclocked).
* Fully non-blocking with zero CPU load during transfer.

---

## GPIO Selection & Hardware Pin Constraints

### 1. ESP8266 (Xtensa lx106)
* **Supported Pins:** **GPIO 0 ... GPIO 15** (driven by fast `W1TS` / `W1TC` hardware registers).
* **Exception — GPIO 16 (D0):** Connected to the RTC peripheral for Deep Sleep wakeups; cannot be addressed via fast GPIO registers and **must not be used** for LED output.
* **SPI Flash Pins:** GPIO 6–11 are connected to the Flash chip and cannot be used.
* **Strapping Pins:**
  * GPIO 0 (D3) and GPIO 2 (D4) must be HIGH at boot (an LED strip pulling to GND forces flash mode).
  * GPIO 15 (D8) must be LOW at boot.
* **Recommended Pins:** **GPIO 5 (D1)** and **GPIO 4 (D2)**. Both are clean and have no boot/system interference.

### 2. ESP32 (Xtensa Dual-Core)
* **Hardware GPIO Matrix:** Connects RMT channel output to any output-capable GPIO pin.
* **Exception — Input-Only Pins (GPI):** **GPIO 34, 35, 36 (VP), 39 (VN)** lack output drivers in hardware and **cannot** be used.
* **SPI Flash Pins:** GPIO 6–11.
* **Strapping Pins:** GPIO 12 (MTDI), GPIO 0, 2, 15.
* **Recommended Pins:** **GPIO 22, 23, 21, 19, 18, 17, 16, 25, 26, 27, 32, 33, 4, 14, 13**.

### 3. ESP32-C3 / C6 / S2 / S3
* **Hardware GPIO Matrix:** Routes RMT to any output GPIO pin.
* **SPI Flash Pins:** Usually GPIO 12–17.
* **USB CDC/JTAG Pins:** GPIO 18, 19 (on C3) — avoid if debugging or flashing via built-in USB-Serial.
* **Strapping Pins:** GPIO 2, 8, 9 (GPIO 9 is BOOT button on C3).
* **Recommended Pins (ESP32-C3):** **GPIO 6, 7, 0, 1, 3, 4, 5, 10, 20, 21**.

### 4. RP2040 / RP2350 (Raspberry Pi Pico)
* **PIO Flexibility:** The PIO State Machine can drive **any GPIO from GPIO 0 to GPIO 29**.
* **Recommended Pins:** **GPIO 0 ... GPIO 22, GPIO 26 ... GPIO 28**.
* On the Raspberry Pi Pico board, pin **GPIO 25** is connected to the onboard LED, while **GPIO 23, 24, 29** are used for SMPS power control and voltage monitoring. Avoid these for LED strips.

---

## Strip Length and LED Count Guidelines

The maximum number of LEDs supported on a single data line depends on 4 factors: **code type limits**, **available RAM**, **WS2812B protocol physical bandwidth (frame time & FPS)**, and **power delivery**.

### Platform Capability Overview

| Platform | Recommended Max (30–60 FPS) | Limit for Slow Effects (5–15 FPS) | Main Limiting Factor |
|---|---|---|---|
| **ESP8266** | **up to 500 – 800 LEDs** | **up to 1,200 – 1,500 LEDs** | Wi-Fi interrupts & Watchdog |
| **ESP32** | **up to 2,000 – 3,000 LEDs** | **up to 10,000 – 15,000 LEDs** | Protocol bandwidth & RAM |
| **ESP32-C3** | **up to 2,000 – 3,000 LEDs** | **up to 6,000 – 8,000 LEDs** | Protocol bandwidth & RAM |
| **RP2040** | **up to 2,000 – 3,000 LEDs** | **up to 10,000 – 15,000 LEDs** | Protocol bandwidth & RAM |

---

### Detailed Analysis of Factors

#### 1. Software & Code Limits
* The LED count variable (`m_num_leds`) is `uint16_t`, giving an architectural limit of **65,535 LEDs**.
* The power limiter `calculate_power_scale` accumulates current into a 32-bit register (`sum_mA_x255`), which only overflows beyond **~400,000 LEDs**.
* The library itself imposes no artificial size limits.

#### 2. Physical Protocol Bandwidth (Frame Time & FPS)
The WS2812B protocol is strictly sequential and fixed at 800 kHz:
* **1 bit = 1.25 µs**
* **1 LED (24 bits) = 30 µs** (0.03 ms)
* Reset / Latch time = **~300 µs** (0.3 ms)

Frame transmission time along a single pin:
$$T_{\text{frame}} = (N \times 0.03\text{ ms}) + 0.3\text{ ms}$$

* **256 LEDs** (32×8 matrix): ~8 ms $\rightarrow$ **up to 125 FPS** (silky smooth animation).
* **512 LEDs**: ~15.7 ms $\rightarrow$ **up to 63 FPS** (smooth).
* **1,000 LEDs**: ~30.3 ms $\rightarrow$ **~33 FPS** (standard cinema refresh rate).
* **2,000 LEDs**: ~60.3 ms $\rightarrow$ **~16 FPS** (fast dynamic animations start losing smoothness).
* **5,000 LEDs**: ~150 ms $\rightarrow$ **~6.6 FPS** (suitable for static illumination or very slow crossfades).

#### 3. Driver Architecture & Wi-Fi Stack

* **ESP32, S2, S3, C3, C6:**
  * Uses the **hardware RMT engine**. FreeRTOS and network interrupts are **never blocked**, and the CPU does not wait in polling loops.
  * The library **does not allocate intermediate RAM buffers** (RMT translation occurs on-the-fly in FIFO ISR directly from `CRGB` array), so memory consumption is only $3\text{ bytes} \times N$.
  * With 200–280 KB free RAM on ESP32, 10,000 LEDs consume only 30 KB. The only bottleneck is physical frame transmission time (~300 ms $\rightarrow$ ~3.3 FPS).

* **ESP8266:**
  * Output is handled by IRAM bitbanging with brief interrupt masking (`noInterrupts()`).
  * For 256 LEDs, interrupts are disabled for only ~7.7 ms — undetectable by the Wi-Fi stack.
  * At **1,000 LEDs**, interrupts are blocked for ~30 ms on every `show()` call. Frequent updates will degrade Wi-Fi responsiveness (higher ping, Telegram/Web delays) and risk Software WDT resets.
  * For ESP8266 running active Wi-Fi, the practical limit is **500 to 800 LEDs**.

#### 4. Power Delivery & Current Consumption
* At 1,000 LEDs, full white color consumes up to **~50–60 Amperes at 5V**.
* The built-in power limiter `setMaxPowerInVoltsAndMilliamps` protects power supplies and wiring by auto-scaling global brightness while preserving exact color hues.
* For strips longer than 300–500 LEDs, **power injection** must be added every 1–2 meters to prevent voltage drops that cause dimming and unwanted reddish tinting towards the end of the strip.

---

## Two Usage Modes

### Mode 1: External Buffer (FastLED Drop-in)
Ideal for projects with complex transition effects (crossfades, slides, scrolling text) using two or more separate buffers:

```cpp
#include <Arduino.h>
#include <FastLED.h> // or #include <TinyLED.h>

#define PIN_LED 5
#define NUM_LEDS 256

CRGB leds[NUM_LEDS];
CRGB ledsNext[NUM_LEDS]; // Second buffer for transition effects

void setup() {
    FastLED.addLeds<WS2812B, PIN_LED, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(50);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000); // 5V, 2000 mA
    FastLED.clear();
    FastLED.show();
}

void loop() {
    fill_rainbow(leds, NUM_LEDS, millis() / 20, 7);
    fadeToBlackBy(leds, NUM_LEDS, 20);
    FastLED.show();
    delay(15);
}
```

### Mode 2: Template Class with Internal Buffer
The simplest and most compact syntax for projects needing only a single LED buffer:

```cpp
#include <Arduino.h>
#include <TinyLED.h>

#define PIN_LED 5
#define NUM_LEDS 64

TinyLED<PIN_LED, NUM_LEDS, ORDER_GRB> strip;

void setup() {
    strip.setCorrection(Correction::TypicalLEDStrip);
    strip.setMaxPower(5, 1500); // 5 Volts, 1500 mA
    strip.setBrightness(100);
}

void loop() {
    strip[0] = CRGB::Red;
    strip[1] = CHSV(millis() / 20, 255, 255);
    strip[2] = blend(strip[0], strip[1], 128);
    strip.show();
    delay(20);
}
```

The `TinyLED` class also allows redirecting output to an external buffer at runtime via `strip.setLeds(ledsNext);`.

---

## Supported FastLED Core Features

* `CRGB`, `CHSV`, `hsv2rgb_rainbow`
* `CRGBPalette16`, `ColorFromPalette`, `PartyColors_p`, `RainbowColors_p`
* `blend`, `nblend`, `fadeToBlackBy`, `fill_rainbow`, `fill_solid`
* `random8`, `random16`, `sin8`, `cos8`, `sin16`, `cos16`, `beatsin8`, `beatsin16`
* `EVERY_N_MILLISECONDS(N)`, `EVERY_N_SECONDS(N)`, `EVERY_N_MINUTES(N)`
* `TypicalLEDStrip` (0xFFB0F0), `setMaxPowerInVoltsAndMilliamps(5, mA)`

---

## License

This library is licensed under the **GNU Lesser General Public License v3.0 (LGPL-3.0-or-later)**. See the [LICENSE](LICENSE) file for complete terms.

Author: **SerhiiLe** (<https://github.com/SerhiiLe>)

