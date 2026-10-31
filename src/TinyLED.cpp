#include "TinyLED.h"

// ============================================================================
// Global State & Palettes
// ============================================================================

uint16_t rand16seed = 1337;
CFastLED FastLED;

const CRGBPalette16 PartyColors_p = {
    0x5500AB, 0x84007C, 0xB5004B, 0xE5001B,
    0xE81700, 0xB84700, 0xAB7700, 0xABAB00,
    0xAB5500, 0xDD2200, 0xF2000E, 0xC2003E,
    0x8F0071, 0x5F00A1, 0x2F00D0, 0x0007F9
};

const CRGBPalette16 RainbowColors_p = {
    0xFF0000, 0xD52A00, 0xAB5500, 0xAB7F00,
    0xABAB00, 0x56AB00, 0x00FF00, 0x00AB55,
    0x0056AB, 0x0000FF, 0x2A00D5, 0x5500AB,
    0x7F0081, 0xAB0055, 0xD5002B, 0xFF0000
};

// ============================================================================
// Trigonometric Functions (Integer Approximation)
// ============================================================================

uint8_t sin8(uint8_t theta) {
    static const uint8_t b_m16_interleave[] = {0, 49, 49, 41, 90, 27, 117, 10};
    uint8_t offset = theta;
    if (theta & 0x40) {
        offset = (uint8_t)255 - offset;
    }
    offset &= 0x3F;

    uint8_t secoffset = offset & 0x0F;
    if (theta & 0x40) ++secoffset;

    uint8_t section = offset >> 4;
    uint8_t s2 = section * 2;
    uint8_t b = b_m16_interleave[s2];
    uint8_t m16 = b_m16_interleave[s2 + 1];

    uint8_t mx = (uint8_t)((m16 * secoffset) >> 4);
    int8_t y = mx + b;
    if (theta & 0x80) y = -y;
    return (uint8_t)(y + 128);
}

int16_t sin16(uint16_t theta) {
    static const uint16_t base[] = {0, 6393, 12539, 18204, 23170, 27245, 30273, 32137};
    static const uint8_t slope[] = {49, 48, 44, 38, 31, 23, 14, 4};

    uint16_t offset = (theta & 0x3FFF) >> 3; // 0..2047
    if (theta & 0x4000) offset = 2047 - offset;

    uint8_t section = offset / 256;
    uint16_t b = base[section];
    uint8_t m = slope[section];

    uint8_t secoffset8 = (uint8_t)(offset) / 2;
    uint16_t mx = m * secoffset8;
    int16_t y = mx + b;

    if (theta & 0x8000) y = -y;
    return y;
}

// ============================================================================
// HSV to RGB Rainbow (Integer Approximation)
// ============================================================================

void hsv2rgb_rainbow(const CHSV& hsv, CRGB& rgb) {
    uint8_t hue = hsv.h;
    uint8_t sat = hsv.s;
    uint8_t val = hsv.v;

    uint8_t offset = hue & 0x1F;
    uint8_t offset8 = offset << 3;
    uint8_t third = scale8(offset8, 85);

    uint8_t r, g, b;

    if (!(hue & 0x80)) {
        if (!(hue & 0x40)) {
            if (!(hue & 0x20)) {
                // Section 0: R -> O
                r = 255 - third;
                g = third;
                b = 0;
            } else {
                // Section 1: O -> Y
                r = 171;
                g = 85 + third;
                b = 0;
            }
        } else {
            if (!(hue & 0x20)) {
                // Section 2: Y -> G
                uint8_t twothirds = scale8(offset8, 170);
                r = 171 - twothirds;
                g = 170 + third;
                b = 0;
            } else {
                // Section 3: G -> A
                r = 0;
                g = 255 - third;
                b = third;
            }
        }
    } else {
        if (!(hue & 0x40)) {
            if (!(hue & 0x20)) {
                // Section 4: A -> B
                uint8_t twothirds = scale8(offset8, 170);
                r = 0;
                g = 171 - twothirds;
                b = 85 + twothirds;
            } else {
                // Section 5: B -> P
                r = third;
                g = 0;
                b = 255 - third;
            }
        } else {
            if (!(hue & 0x20)) {
                // Section 6: P -> K
                r = 85 + third;
                g = 0;
                b = 171 - third;
            } else {
                // Section 7: K -> R
                r = 170 + third;
                g = 0;
                b = 85 - third;
            }
        }
    }

    // Apply saturation
    if (sat != 255) {
        if (sat == 0) {
            r = 255; g = 255; b = 255;
        } else {
            uint8_t desat = 255 - sat;
            desat = scale8_video(desat, desat);
            uint8_t satscale = 255 - desat;
            if (r) r = scale8(r, satscale) + 1;
            if (g) g = scale8(g, satscale) + 1;
            if (b) b = scale8(b, satscale) + 1;
            r += desat;
            g += desat;
            b += desat;
        }
    }

    // Apply brightness (val)
    if (val != 255) {
        val = scale8_video(val, val);
        if (val == 0) {
            r = 0; g = 0; b = 0;
        } else {
            if (r) r = scale8(r, val) + 1;
            if (g) g = scale8(g, val) + 1;
            if (b) b = scale8(b, val) + 1;
        }
    }

    rgb.r = r;
    rgb.g = g;
    rgb.b = b;
}

// ============================================================================
// Palette Interpolation
// ============================================================================

CRGB ColorFromPalette(const CRGBPalette16& pal, uint8_t index, uint8_t brightness, TBlendType blendType) {
    if (blendType == LINEARBLEND_NOWRAP && index > 239) {
        index = (uint8_t)(((uint16_t)index * 239) / 255);
    }

    uint8_t hi4 = index >> 4;
    uint8_t lo4 = index & 0x0F;

    CRGB entry1 = pal.entries[hi4];
    uint8_t r = entry1.r;
    uint8_t g = entry1.g;
    uint8_t b = entry1.b;

    if (lo4 && blendType != NOBLEND) {
        uint8_t nextIdx = (hi4 == 15) ? 0 : (hi4 + 1);
        CRGB entry2 = pal.entries[nextIdx];
        uint8_t f2 = lo4 << 4;
        uint8_t f1 = 255 - f2;
        r = scale8(r, f1) + scale8(entry2.r, f2);
        g = scale8(g, f1) + scale8(entry2.g, f2);
        b = scale8(b, f1) + scale8(entry2.b, f2);
    }

    if (brightness < 255) {
        r = scale8_video(r, brightness);
        g = scale8_video(g, brightness);
        b = scale8_video(b, brightness);
    }

    return CRGB(r, g, b);
}

// ============================================================================
// Power Limiter Calculation
// ============================================================================

static uint8_t calculate_power_scale(const CRGB* leds, uint16_t num_leds, uint8_t brightness,
                                     uint8_t corr_r, uint8_t corr_g, uint8_t corr_b,
                                     uint32_t max_mA) {
    if (brightness == 0 || max_mA == 0 || num_leds == 0 || !leds) return brightness;

    uint32_t idle_mA = (uint32_t)num_leds * 1; // ~1 mA quiescent per LED
    if (idle_mA >= max_mA) return (brightness > 0) ? 1 : 0;

    uint32_t avail_mA = max_mA - idle_mA;

    uint32_t sum_mA_x255 = 0;
    uint8_t scale_r = scale8_video(brightness, corr_r);
    uint8_t scale_g = scale8_video(brightness, corr_g);
    uint8_t scale_b = scale8_video(brightness, corr_b);

    for (uint16_t i = 0; i < num_leds; i++) {
        uint8_t r = scale8_video(leds[i].r, scale_r);
        uint8_t g = scale8_video(leds[i].g, scale_g);
        uint8_t b = scale8_video(leds[i].b, scale_b);

        // At 5V: R: 16 mA, G: 11 mA, B: 15 mA
        sum_mA_x255 += ((uint32_t)r * 16 + (uint32_t)g * 11 + (uint32_t)b * 15);
    }

    uint32_t leds_mA = sum_mA_x255 / 255;
    if (leds_mA <= avail_mA) {
        return brightness;
    }

    uint32_t adjusted = ((uint32_t)brightness * avail_mA) / leds_mA;
    if (adjusted == 0 && brightness > 0) {
        adjusted = 1;
    }
    return (uint8_t)(adjusted > 255 ? 255 : adjusted);
}

// ============================================================================
// Reset timing helper
// ============================================================================

static uint32_t s_last_show_us = 0;

static inline void ws2812_wait_reset() {
    uint32_t elapsed = micros() - s_last_show_us;
    if (elapsed < 300) {
        delayMicroseconds(300 - elapsed);
    }
}

static inline void ws2812_mark_reset() {
    s_last_show_us = micros();
}

// ============================================================================
// Platform Drivers
// ============================================================================

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32) || defined(CONFIG_IDF_TARGET_ESP32) || \
    defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || \
    defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
    defined(CONFIG_IDF_TARGET_ESP32H2)
  #define _TINYLED_PLATFORM_ESP32 1
#elif defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350) || defined(ARDUINO_ARCH_PICO) || \
      defined(PICO_RP2040) || defined(PICO_RP2350) || defined(TARGET_RP2040) || defined(TARGET_RP2350) || \
      defined(RP2040) || defined(RP2350)
  #define _TINYLED_PLATFORM_RP2040 1
#elif defined(ESP8266)
  #define _TINYLED_PLATFORM_ESP8266 1
#endif

#if defined(_TINYLED_PLATFORM_ESP8266)

struct FASTLED_ESP_IO {
    volatile uint32_t _GPO;
    volatile uint32_t _GPOS;
    volatile uint32_t _GPOC;
};
#define _GPB (*(FASTLED_ESP_IO*)(0x60000000+(0x300)))

// Info on reading cycle counter from https://github.com/kbeckmann/nodemcu-firmware/blob/ws2812-dual/app/modules/ws2812.c
__attribute__ ((always_inline)) inline static uint32_t __clock_cycles() {
    uint32_t cyc;
    __asm__ __volatile__ ("rsr %0,ccount":"=a" (cyc));
    return cyc;
}

template<int BITS, uint32_t T1, uint32_t T2, uint32_t T3>
__attribute__ ((always_inline)) inline static bool writeBits(
    uint32_t & last_mark,
    uint32_t b,
    uint32_t mask)
{
    b <<= 24; b = ~b;
    for(uint32_t i = BITS; i > 0; --i) {
        while((__clock_cycles() - last_mark) < (T1+T2+T3)) {
            ;
        }
        last_mark = __clock_cycles();
        _GPB._GPOS = mask;

        while((__clock_cycles() - last_mark) < T1) {
            ;
        }
        if(b & 0x80000000L) { _GPB._GPOC = mask; }
        b <<= 1;

        while((__clock_cycles() - last_mark) < (T1+T2)) {
            ;
        }
        _GPB._GPOC = mask;

        // even with interrupts disabled, the NMI interupt seems to cause
        // timing issues here. abort the frame if one bit took to long. if the
        // last of the 24 bits has been sent already, it is too late
        // this fixes the flickering first pixel that started to occur with
        // framework version 3.0.0
        if ((__clock_cycles() - last_mark) >= (T1 + T2 + T3 - 5)) {
            return true;
        }
    }
    return false;
}

template<uint32_t T1, uint32_t T2, uint32_t T3>
static uint32_t IRAM_ATTR showRGBInternal_esp8266_templated(
    const uint8_t* pixels, uint16_t num_leds, uint32_t clks_per_us, uint32_t mask)
{
    const uint8_t* p = pixels;
    uint32_t b = *p++;
    uint32_t start;

    struct InterruptLock {
        InterruptLock()  { ets_intr_lock(); }
        ~InterruptLock() { ets_intr_unlock(); }
        void Unlock()    { ets_intr_unlock(); }
        void Lock()      { ets_intr_lock(); }
    };

    {
        InterruptLock intlock;
        start = __clock_cycles();
        uint32_t last_mark = start;

        for (uint16_t i = 0; i < num_leds; i++) {
            // Write first byte, read second byte
            if (writeBits<8, T1, T2, T3>(last_mark, b, mask)) {
                _GPB._GPOC = mask;
                return 0;
            }
            b = *p++;

            // Write second byte, read third byte
            if (writeBits<8, T1, T2, T3>(last_mark, b, mask)) {
                _GPB._GPOC = mask;
                return 0;
            }
            b = *p++;

            // Write third byte
            if (writeBits<8, T1, T2, T3>(last_mark, b, mask)) {
                _GPB._GPOC = mask;
                return 0;
            }

            // Between pixels: momentarily allow interrupts for Wi-Fi stack
            intlock.Unlock();
            if (i + 1 < num_leds) {
                b = *p++;
            }
            intlock.Lock();

            // If interrupts took longer than 45 us, WS2812 may latch: abort and retry
            if ((int32_t)(__clock_cycles() - last_mark) > 0) {
                if ((int32_t)(__clock_cycles() - last_mark) > (int32_t)(T1 + T2 + T3 + (45 * clks_per_us))) {
                    _GPB._GPOC = mask;
                    return 0;
                }
            }
        }
    }

    return __clock_cycles() - start;
}

static uint32_t IRAM_ATTR showRGBInternal_esp8266(
    const uint8_t* pixels, uint16_t num_leds, uint32_t mask)
{
    uint32_t f_cpu = (uint32_t)ESP.getCpuFreqMHz() * 1000000UL;
    if (f_cpu >= 160000000L) {
        // 160 MHz: FMUL=20 -> T1=40, T2=100, T3=60, clks_per_us=160
        return showRGBInternal_esp8266_templated<40, 100, 60>(pixels, num_leds, 160, mask);
    } else {
        // 80 MHz: FMUL=10 -> T1=20, T2=50, T3=30, clks_per_us=80
        return showRGBInternal_esp8266_templated<20, 50, 30>(pixels, num_leds, 80, mask);
    }
}

static void IRAM_ATTR ws2812_esp8266_send(uint8_t pin, const CRGB* leds, uint16_t num_leds,
                                          uint8_t scale_r, uint8_t scale_g, uint8_t scale_b,
                                          EOrder order) {
    if (num_leds == 0 || !leds) return;

    uint32_t mask = 1UL << pin;
    size_t num_bytes = (size_t)num_leds * 3;

    // Buffer preparation:
    static uint8_t s_local_buf[1024]; // 1 KB fast buffer supports up to 341 LEDs without allocation
    uint8_t* tx_buf = s_local_buf;
    bool allocated = false;

    if (num_bytes > sizeof(s_local_buf)) {
        tx_buf = (uint8_t*)malloc(num_bytes);
        if (!tx_buf) return;
        allocated = true;
    }

    uint8_t* dst = tx_buf;
    for (uint16_t i = 0; i < num_leds; i++) {
        uint8_t r = scale8_video(leds[i].r, scale_r);
        uint8_t g = scale8_video(leds[i].g, scale_g);
        uint8_t b = scale8_video(leds[i].b, scale_b);

        switch (order) {
            case RGB: *dst++ = r; *dst++ = g; *dst++ = b; break;
            case RBG: *dst++ = r; *dst++ = b; *dst++ = g; break;
            case BGR: *dst++ = b; *dst++ = g; *dst++ = r; break;
            case BRG: *dst++ = b; *dst++ = r; *dst++ = g; break;
            case GBR: *dst++ = g; *dst++ = b; *dst++ = r; break;
            default:  *dst++ = g; *dst++ = r; *dst++ = b; break; // GRB
        }
    }

    // FastLED-identical retry loop with NMI & Wi-Fi collision abort
    int cnt = 3;
    while ((showRGBInternal_esp8266(tx_buf, num_leds, mask) == 0) && cnt--) {
        _GPB._GPOC = mask;
        delayMicroseconds(300);
    }

    if (allocated) {
        free(tx_buf);
    }
}

#elif defined(_TINYLED_PLATFORM_ESP32)

#include "esp_idf_version.h"

struct RmtTxContext {
    const CRGB* leds;
    uint8_t scale_r;
    uint8_t scale_g;
    uint8_t scale_b;
    EOrder order;
};

static RmtTxContext s_rmt_ctx;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
// ============================================================================
// Modern ESP-IDF 5.x RMT Driver (driver/rmt_tx.h, driver/rmt_encoder.h)
// ============================================================================
#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>

static rmt_channel_handle_t s_tx_channel = NULL;
static rmt_encoder_handle_t s_simple_encoder = NULL;
static uint8_t s_installed_pin = 255;

static inline uint8_t IRAM_ATTR get_channel_byte(const CRGB* leds, size_t pixel_idx, size_t channel_idx) {
    const CRGB& p = leds[pixel_idx];
    uint8_t r = scale8_video(p.r, s_rmt_ctx.scale_r);
    uint8_t g = scale8_video(p.g, s_rmt_ctx.scale_g);
    uint8_t b = scale8_video(p.b, s_rmt_ctx.scale_b);

    switch (s_rmt_ctx.order) {
        case RGB: return (channel_idx == 0) ? r : (channel_idx == 1 ? g : b);
        case RBG: return (channel_idx == 0) ? r : (channel_idx == 1 ? b : g);
        case BGR: return (channel_idx == 0) ? b : (channel_idx == 1 ? g : r);
        case BRG: return (channel_idx == 0) ? b : (channel_idx == 1 ? r : g);
        case GBR: return (channel_idx == 0) ? g : (channel_idx == 1 ? b : r);
        default:  return (channel_idx == 0) ? g : (channel_idx == 1 ? r : b); // GRB
    }
}

static size_t IRAM_ATTR ws2812_simple_encode_cb(const void *data, size_t data_size,
                                                size_t symbols_written, size_t symbols_free,
                                                rmt_symbol_word_t *symbols, bool *done, void *arg) {
    (void)arg;
    size_t data_symbols = data_size * 8;
    size_t total_symbols = data_symbols + 1; // +1 trailing reset symbol (300 us LOW)

    if (symbols_written >= total_symbols) {
        *done = true;
        return 0;
    }

    size_t symbols_to_write = total_symbols - symbols_written;
    if (symbols_to_write > symbols_free) {
        symbols_to_write = symbols_free;
    }

    const CRGB* leds = (const CRGB*)data;

    // At 10 MHz resolution (100 ns per tick):
    // Bit 0: 300 ns HIGH (3 ticks), 900 ns LOW (9 ticks) -> 1.2 us
    // Bit 1: 900 ns HIGH (9 ticks), 300 ns LOW (3 ticks) -> 1.2 us
    // Reset: 300 us LOW (1500 ticks + 1500 ticks = 3000 ticks = 300 us)
    rmt_symbol_word_t bit0;
    bit0.duration0 = 3; bit0.level0 = 1; bit0.duration1 = 9; bit0.level1 = 0;
    rmt_symbol_word_t bit1;
    bit1.duration0 = 9; bit1.level0 = 1; bit1.duration1 = 3; bit1.level1 = 0;
    rmt_symbol_word_t reset_sym;
    reset_sym.duration0 = 1500; reset_sym.level0 = 0; reset_sym.duration1 = 1500; reset_sym.level1 = 0;

    size_t current_sym = symbols_written;
    size_t byte_idx = current_sym >> 3;
    size_t bit_idx = current_sym & 7;
    size_t pixel_idx = byte_idx / 3;
    size_t channel_idx = byte_idx % 3;

    uint8_t cur_byte_val = 0;
    if (current_sym < data_symbols) {
        cur_byte_val = get_channel_byte(leds, pixel_idx, channel_idx);
    }

    for (size_t i = 0; i < symbols_to_write; i++, current_sym++) {
        if (current_sym < data_symbols) {
            symbols[i] = (cur_byte_val & (0x80 >> bit_idx)) ? bit1 : bit0;
            if (++bit_idx == 8) {
                bit_idx = 0;
                if (++channel_idx == 3) {
                    channel_idx = 0;
                    pixel_idx++;
                }
                if (current_sym + 1 < data_symbols) {
                    cur_byte_val = get_channel_byte(leds, pixel_idx, channel_idx);
                }
            }
        } else {
            symbols[i] = reset_sym;
        }
    }

    *done = ((symbols_written + symbols_to_write) >= total_symbols);
    return symbols_to_write;
}

static void ws2812_rmt_init(uint8_t pin) {
    if (s_tx_channel && s_installed_pin == pin) return;

    if (s_tx_channel) {
        rmt_disable(s_tx_channel);
        if (s_simple_encoder) {
            rmt_del_encoder(s_simple_encoder);
            s_simple_encoder = NULL;
        }
        rmt_del_channel(s_tx_channel);
        s_tx_channel = NULL;
    }

    rmt_tx_channel_config_t tx_chan_config = {};
    tx_chan_config.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_chan_config.gpio_num = (gpio_num_t)pin;
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP32C3)
    tx_chan_config.mem_block_symbols = 96; // Double FIFO depth on ESP32-C3 for Wi-Fi safety
#elif defined(SOC_RMT_MEM_WORDS_PER_CHANNEL)
    tx_chan_config.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL;
#else
    tx_chan_config.mem_block_symbols = 64;
#endif
    tx_chan_config.resolution_hz = 10000000; // 10 MHz (100 ns tick)
    tx_chan_config.trans_queue_depth = 4;
    tx_chan_config.intr_priority = 3; // High priority interrupt to prevent Wi-Fi starvation
    tx_chan_config.flags.invert_out = 0;
    tx_chan_config.flags.with_dma = 0;

    if (rmt_new_tx_channel(&tx_chan_config, &s_tx_channel) == ESP_OK) {
        rmt_simple_encoder_config_t enc_config = {};
        enc_config.callback = ws2812_simple_encode_cb;
        enc_config.arg = NULL;
        enc_config.min_chunk_size = 0; // Allow any small chunk refill

        if (rmt_new_simple_encoder(&enc_config, &s_simple_encoder) == ESP_OK &&
            rmt_enable(s_tx_channel) == ESP_OK) {
            s_installed_pin = pin;
        }
    }
}

static void ws2812_rmt_send(const CRGB* leds, uint16_t num_leds,
                            uint8_t scale_r, uint8_t scale_g, uint8_t scale_b,
                            EOrder order) {
    if (!s_tx_channel || !s_simple_encoder) return;

    s_rmt_ctx.leds = leds;
    s_rmt_ctx.scale_r = scale_r;
    s_rmt_ctx.scale_g = scale_g;
    s_rmt_ctx.scale_b = scale_b;
    s_rmt_ctx.order = order;

    rmt_transmit_config_t transmit_config = {};
    transmit_config.loop_count = 0;
    transmit_config.flags.eot_level = 0;

    if (rmt_transmit(s_tx_channel, s_simple_encoder, leds, num_leds * sizeof(CRGB), &transmit_config) == ESP_OK) {
        rmt_tx_wait_all_done(s_tx_channel, -1);
    }
}

#else
// ============================================================================
// Legacy ESP-IDF 4.x RMT Driver (driver/rmt.h)
// ============================================================================
#include <driver/rmt.h>

static const rmt_item32_t s_bit0 = {{{ 14, 1, 36, 0 }}}; // 350ns HIGH, 900ns LOW
static const rmt_item32_t s_bit1 = {{{ 32, 1, 18, 0 }}}; // 800ns HIGH, 450ns LOW

static void IRAM_ATTR ws2812_rmt_adapter(const void *src, rmt_item32_t *dest, size_t src_size,
                                         size_t wanted_num, size_t *translated_size, size_t *item_num) {
    if (!src || !dest) {
        *translated_size = 0;
        *item_num = 0;
        return;
    }

    size_t bytes_done = 0;
    size_t items_done = 0;
    const uint8_t *psrc = (const uint8_t *)src;
    rmt_item32_t *pdest = dest;

    while (bytes_done < src_size && (items_done + 8) <= wanted_num) {
        size_t byte_offset = (psrc + bytes_done) - (const uint8_t *)s_rmt_ctx.leds;
        size_t pixel_idx = byte_offset / 3;
        size_t channel_idx = byte_offset % 3;

        const CRGB& p = s_rmt_ctx.leds[pixel_idx];
        uint8_t r = scale8_video(p.r, s_rmt_ctx.scale_r);
        uint8_t g = scale8_video(p.g, s_rmt_ctx.scale_g);
        uint8_t b = scale8_video(p.b, s_rmt_ctx.scale_b);

        uint8_t c0, c1, c2;
        switch (s_rmt_ctx.order) {
            case RGB: c0 = r; c1 = g; c2 = b; break;
            case RBG: c0 = r; c1 = b; c2 = g; break;
            case BGR: c0 = b; c1 = g; c2 = r; break;
            case BRG: c0 = b; c1 = r; c2 = g; break;
            case GBR: c0 = g; c1 = b; c2 = r; break;
            default:  // GRB
                      c0 = g; c1 = r; c2 = b; break;
        }

        uint8_t byte_val;
        if (channel_idx == 0) byte_val = c0;
        else if (channel_idx == 1) byte_val = c1;
        else byte_val = c2;

        for (int bit = 7; bit >= 0; bit--) {
            *pdest++ = (byte_val & (1 << bit)) ? s_bit1 : s_bit0;
        }

        items_done += 8;
        bytes_done++;
    }

    *translated_size = bytes_done;
    *item_num = items_done;
}

static bool s_rmt_installed = false;
static uint8_t s_installed_pin = 255;

static void ws2812_rmt_init(uint8_t pin) {
    if (s_rmt_installed && s_installed_pin == pin) return;

    if (s_rmt_installed) {
        rmt_driver_uninstall(RMT_CHANNEL_0);
        s_rmt_installed = false;
    }

    rmt_config_t config;
    config.rmt_mode = RMT_MODE_TX;
    config.channel = RMT_CHANNEL_0;
    config.gpio_num = (gpio_num_t)pin;
    config.mem_block_num = 1;
    config.clk_div = 2; // 80 MHz / 2 = 40 MHz -> 25 ns/tick
    config.tx_config.loop_en = false;
    config.tx_config.carrier_en = false;
    config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    config.tx_config.idle_output_en = true;

    if (rmt_config(&config) == ESP_OK &&
        rmt_driver_install(config.channel, 0, 0) == ESP_OK &&
        rmt_translator_init(config.channel, ws2812_rmt_adapter) == ESP_OK) {
        s_rmt_installed = true;
        s_installed_pin = pin;
    }
}

static void ws2812_rmt_send(const CRGB* leds, uint16_t num_leds,
                            uint8_t scale_r, uint8_t scale_g, uint8_t scale_b,
                            EOrder order) {
    s_rmt_ctx.leds = leds;
    s_rmt_ctx.scale_r = scale_r;
    s_rmt_ctx.scale_g = scale_g;
    s_rmt_ctx.scale_b = scale_b;
    s_rmt_ctx.order = order;

    rmt_write_sample(RMT_CHANNEL_0, (const uint8_t *)leds, num_leds * 3, true);
}

#endif // ESP_IDF_VERSION >= 5.0.0

#elif defined(_TINYLED_PLATFORM_RP2040)
// ============================================================================
// Raspberry Pi Pico (RP2040) Hardware PIO Driver
// ============================================================================
#include "hardware/pio.h"
#include "hardware/clocks.h"

#define WS2812_T1 3
#define WS2812_T2 3
#define WS2812_T3 4

static const uint16_t ws2812_program_instructions[] = {
    // .wrap_target
    0x6321, // 0: out    x, 1           side 0 [3]
    0x1223, // 1: jmp    !x, 3          side 1 [2]
    0x1200, // 2: jmp    0              side 1 [2]
    0xa242  // 3: nop                   side 0 [2]
    // .wrap
};

static const struct pio_program ws2812_program = {
    .instructions = ws2812_program_instructions,
    .length = 4,
    .origin = -1,
};

static inline pio_sm_config ws2812_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + 3);
    sm_config_set_sideset(&c, 1, false, false);
    return c;
}

static PIO s_rp2040_pio = nullptr;
static int s_rp2040_sm = -1;
static uint s_rp2040_offset = 0;
static uint8_t s_installed_pin = 255;

static void ws2812_rp2040_init(uint8_t pin) {
    if (s_rp2040_pio && s_installed_pin == pin) return;

    if (s_rp2040_pio && s_rp2040_sm >= 0) {
        pio_sm_set_enabled(s_rp2040_pio, s_rp2040_sm, false);
        pio_sm_unclaim(s_rp2040_pio, s_rp2040_sm);
        s_rp2040_sm = -1;
    }

    if (!s_rp2040_pio) {
        if (pio_can_add_program(pio0, &ws2812_program)) {
            s_rp2040_pio = pio0;
            s_rp2040_offset = pio_add_program(pio0, &ws2812_program);
        } else if (pio_can_add_program(pio1, &ws2812_program)) {
            s_rp2040_pio = pio1;
            s_rp2040_offset = pio_add_program(pio1, &ws2812_program);
        } else {
            return;
        }
    }

    s_rp2040_sm = pio_claim_unused_sm(s_rp2040_pio, false);
    if (s_rp2040_sm < 0) {
        PIO other = pio0;
        if (s_rp2040_pio == pio0) other = pio1;
        if (pio_can_add_program(other, &ws2812_program)) {
            s_rp2040_pio = other;
            s_rp2040_offset = pio_add_program(other, &ws2812_program);
            s_rp2040_sm = pio_claim_unused_sm(other, false);
        }
    }
    if (s_rp2040_sm < 0) return;

    pio_gpio_init(s_rp2040_pio, pin);
    pio_sm_set_consecutive_pindirs(s_rp2040_pio, s_rp2040_sm, pin, 1, true);

    pio_sm_config c = ws2812_program_get_default_config(s_rp2040_offset);
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_out_shift(&c, false, true, 24); // Shift MSB first, autopull 24 bits
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    int cycles_per_bit = WS2812_T1 + WS2812_T2 + WS2812_T3;
    float div = (float)clock_get_hz(clk_sys) / (800000.0f * (float)cycles_per_bit);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(s_rp2040_pio, s_rp2040_sm, s_rp2040_offset, &c);
    pio_sm_set_enabled(s_rp2040_pio, s_rp2040_sm, true);
    s_installed_pin = pin;
}

static void ws2812_rp2040_send(const CRGB* leds, uint16_t num_leds,
                              uint8_t scale_r, uint8_t scale_g, uint8_t scale_b,
                              EOrder order) {
    if (!s_rp2040_pio || s_rp2040_sm < 0 || !leds || num_leds == 0) return;

    for (uint16_t i = 0; i < num_leds; i++) {
        uint8_t r = scale8_video(leds[i].r, scale_r);
        uint8_t g = scale8_video(leds[i].g, scale_g);
        uint8_t b = scale8_video(leds[i].b, scale_b);

        uint8_t c0, c1, c2;
        switch (order) {
            case RGB: c0 = r; c1 = g; c2 = b; break;
            case RBG: c0 = r; c1 = b; c2 = g; break;
            case BGR: c0 = b; c1 = g; c2 = r; break;
            case BRG: c0 = b; c1 = r; c2 = g; break;
            case GBR: c0 = g; c1 = b; c2 = r; break;
            default:  // GRB (WS2812B standard)
                      c0 = g; c1 = r; c2 = b; break;
        }

        uint32_t word = ((uint32_t)c0 << 24) | ((uint32_t)c1 << 16) | ((uint32_t)c2 << 8);
        pio_sm_put_blocking(s_rp2040_pio, s_rp2040_sm, word);
    }

    // Wait until TX FIFO is completely drained
    while (!pio_sm_is_tx_fifo_empty(s_rp2040_pio, s_rp2040_sm)) {
        tight_loop_contents();
    }
}

#endif // Platform drivers

// ============================================================================
// TinyLEDController Implementation
// ============================================================================

TinyLEDController::TinyLEDController(uint8_t pin, uint16_t num_leds, CRGB* leds, EOrder order)
    : m_pin(pin), m_num_leds(num_leds), m_leds(leds), m_order(order),
      m_brightness(255), m_correction(TypicalLEDStrip),
      m_power_volts(5), m_max_power_mA(0), m_initialized(false) {
    if (m_num_leds > 0 && m_leds != nullptr) {
        init(pin, num_leds, leds, order);
    }
}

void TinyLEDController::init(uint8_t pin, uint16_t num_leds, CRGB* leds, EOrder order) {
    m_pin = pin;
    m_num_leds = num_leds;
    m_leds = leds;
    m_order = order;

#if defined(_TINYLED_PLATFORM_ESP8266)
    pinMode(m_pin, OUTPUT);
    digitalWrite(m_pin, LOW);
    ws2812_mark_reset();
    delayMicroseconds(300);
#elif defined(_TINYLED_PLATFORM_ESP32)
    ws2812_rmt_init(m_pin);
#elif defined(_TINYLED_PLATFORM_RP2040)
    ws2812_rp2040_init(m_pin);
#endif

    m_initialized = true;
}

TinyLEDController& TinyLEDController::setCorrection(uint32_t corr) {
    m_correction = CRGB(corr);
    return *this;
}

TinyLEDController& TinyLEDController::setCorrection(const CRGB& corr) {
    m_correction = corr;
    return *this;
}

void TinyLEDController::setBrightness(uint8_t b) {
    m_brightness = b;
}

void TinyLEDController::setMaxPowerInVoltsAndMilliamps(uint8_t volts, uint32_t milliamps) {
    m_power_volts = volts;
    m_max_power_mA = milliamps;
}

void TinyLEDController::clear(bool writeData) {
    clearData();
    if (writeData) {
        show();
    }
}

void TinyLEDController::clearData() {
    if (m_leds && m_num_leds > 0) {
        memset(reinterpret_cast<void*>(m_leds), 0, m_num_leds * sizeof(CRGB));
    }
}

void TinyLEDController::show() {
    if (!m_initialized || !m_leds || m_num_leds == 0) return;

#if defined(_TINYLED_PLATFORM_ESP32)
    ws2812_rmt_init(m_pin);
#elif defined(_TINYLED_PLATFORM_RP2040)
    ws2812_rp2040_init(m_pin);
#endif

    ws2812_wait_reset();

    uint8_t effective_brightness = m_brightness;
    if (m_max_power_mA > 0) {
        effective_brightness = calculate_power_scale(m_leds, m_num_leds, m_brightness,
                                                     m_correction.r, m_correction.g, m_correction.b,
                                                     m_max_power_mA);
    }

    uint8_t scale_r = scale8_video(effective_brightness, m_correction.r);
    uint8_t scale_g = scale8_video(effective_brightness, m_correction.g);
    uint8_t scale_b = scale8_video(effective_brightness, m_correction.b);

#if defined(_TINYLED_PLATFORM_ESP8266)
    ws2812_esp8266_send(m_pin, m_leds, m_num_leds, scale_r, scale_g, scale_b, m_order);
#elif defined(_TINYLED_PLATFORM_ESP32)
    ws2812_rmt_send(m_leds, m_num_leds, scale_r, scale_g, scale_b, m_order);
#elif defined(_TINYLED_PLATFORM_RP2040)
    ws2812_rp2040_send(m_leds, m_num_leds, scale_r, scale_g, scale_b, m_order);
#endif

    ws2812_mark_reset();
}
