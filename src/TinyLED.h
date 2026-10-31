#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

// ============================================================================
// Types & Definitions
// ============================================================================

typedef uint8_t fract8;
typedef uint16_t accum88;

enum TBlendType {
    NOBLEND = 0,
    LINEARBLEND = 1,
    LINEARBLEND_NOWRAP = 2
};

enum EOrder {
    ORDER_RGB = 0012, RGB = 0012,
    ORDER_RBG = 0021, RBG = 0021,
    ORDER_GRB = 0102, GRB = 0102,
    ORDER_GBR = 0120, GBR = 0120,
    ORDER_BRG = 0201, BRG = 0201,
    ORDER_BGR = 0210, BGR = 0210
};

struct Correction {
    enum Type {
        UncorrectedColor = 0xFFFFFF,
        TypicalLEDStrip = 0xFFB0F0
    };
};
static const uint32_t UncorrectedColor = Correction::UncorrectedColor;
static const uint32_t TypicalLEDStrip = Correction::TypicalLEDStrip;

struct WS2812B {};
struct WS2812 {};
struct WS2811 {};
struct NEOPIXEL {};

// ============================================================================
// Fast Math & Utility Inlines
// ============================================================================

static inline uint8_t scale8(uint8_t i, fract8 scale) {
    return (uint8_t)(((uint16_t)i * (uint16_t)scale) >> 8);
}

static inline uint8_t scale8_video(uint8_t i, fract8 scale) {
    return (uint8_t)((((uint16_t)i * (uint16_t)scale) >> 8) + ((i && scale) ? 1 : 0));
}

static inline uint16_t scale16(uint16_t i, fract8 scale) {
    return (uint16_t)(((uint32_t)i * (uint32_t)scale) >> 8);
}

static inline uint16_t scale16(uint16_t i, uint16_t scale) {
    return (uint16_t)(((uint32_t)i * (uint32_t)scale) >> 16);
}

static inline uint8_t qadd8(uint8_t i, uint8_t j) {
    uint16_t t = (uint16_t)i + (uint16_t)j;
    return (t > 255) ? 255 : (uint8_t)t;
}

static inline uint8_t qsub8(uint8_t i, uint8_t j) {
    int t = (int)i - (int)j;
    return (t < 0) ? 0 : (uint8_t)t;
}

static inline uint8_t blend8(uint8_t a, uint8_t b, fract8 amountOfB) {
    if (amountOfB == 255) return b;
    if (amountOfB == 0) return a;
    uint16_t partial = (uint16_t)a * (256 - amountOfB) + (uint16_t)b * amountOfB;
    return (uint8_t)(partial >> 8);
}

// ============================================================================
// Random Number Generator (FastLED compatible LCG)
// ============================================================================

extern uint16_t rand16seed;

static inline uint8_t random8() {
    rand16seed = (uint16_t)(rand16seed * 2053 + 13849);
    return (uint8_t)((rand16seed & 0xFF) + (rand16seed >> 8));
}

static inline uint8_t random8(uint8_t lim) {
    if (lim == 0) return 0;
    return (uint8_t)(((uint16_t)random8() * lim) >> 8);
}

static inline uint8_t random8(uint8_t min, uint8_t lim) {
    if (lim <= min) return min;
    return min + random8(lim - min);
}

static inline uint16_t random16() {
    rand16seed = (uint16_t)(rand16seed * 2053 + 13849);
    return rand16seed;
}

static inline uint16_t random16(uint16_t lim) {
    if (lim == 0) return 0;
    return (uint16_t)(((uint32_t)random16() * (uint32_t)lim) >> 16);
}

static inline uint16_t random16(uint16_t min, uint16_t lim) {
    if (lim <= min) return min;
    return min + random16(lim - min);
}

static inline void random16_set_seed(uint16_t seed) { rand16seed = seed; }
static inline uint16_t random16_get_seed() { return rand16seed; }

// ============================================================================
// Trigonometric & Beat Functions
// ============================================================================

uint8_t sin8(uint8_t theta);
static inline uint8_t cos8(uint8_t theta) { return sin8(theta + 64); }

int16_t sin16(uint16_t theta);
static inline int16_t cos16(uint16_t theta) { return sin16(theta + 16384); }

static inline uint16_t beat88(accum88 beats_per_minute_88, uint32_t timebase = 0) {
    return (uint16_t)((((uint32_t)(millis() - timebase)) * beats_per_minute_88 * 280UL) >> 16);
}

static inline uint16_t beat16(accum88 beats_per_minute, uint32_t timebase = 0) {
    if (beats_per_minute < 256) beats_per_minute <<= 8;
    return beat88(beats_per_minute, timebase);
}

static inline uint8_t beat8(accum88 beats_per_minute, uint32_t timebase = 0) {
    return (uint8_t)(beat16(beats_per_minute, timebase) >> 8);
}

static inline uint8_t beatsin8(accum88 beats_per_minute, uint8_t lowest = 0, uint8_t highest = 255,
                              uint32_t timebase = 0, uint8_t phase_offset = 0) {
    uint8_t beat = beat8(beats_per_minute, timebase);
    uint8_t beatsin = sin8(beat + phase_offset);
    uint8_t rangewidth = highest - lowest;
    return lowest + scale8(beatsin, rangewidth);
}

static inline uint16_t beatsin16(accum88 beats_per_minute, uint16_t lowest = 0, uint16_t highest = 65535,
                                uint32_t timebase = 0, uint16_t phase_offset = 0) {
    uint16_t beat = beat16(beats_per_minute, timebase);
    uint16_t beatsin = (uint16_t)(sin16(beat + phase_offset) + 32768);
    uint16_t rangewidth = highest - lowest;
    return lowest + scale16(beatsin, rangewidth);
}

// ============================================================================
// Timing Macros
// ============================================================================

#define _TL_CONCAT_INNER(a, b) a ## b
#define _TL_CONCAT(a, b) _TL_CONCAT_INNER(a, b)

#define EVERY_N_MILLIS_I(NAME, N) \
    static uint32_t NAME = 0; \
    if (millis() - NAME >= (N)) if (((NAME = millis()) || 1))

#define EVERY_N_MILLISECONDS(N) EVERY_N_MILLIS_I(_TL_CONCAT(_timer_ms_, __LINE__), N)
#define EVERY_N_SECONDS(N) EVERY_N_MILLIS_I(_TL_CONCAT(_timer_s_, __LINE__), (uint32_t)(N) * 1000UL)
#define EVERY_N_MINUTES(N) EVERY_N_MILLIS_I(_TL_CONCAT(_timer_m_, __LINE__), (uint32_t)(N) * 60000UL)

// ============================================================================
// CRGB & CHSV Structures
// ============================================================================

struct CHSV;

struct __attribute__((packed)) CRGB {
    union {
        struct {
            uint8_t r;
            uint8_t g;
            uint8_t b;
        };
        struct {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
        };
        uint8_t raw[3];
    };

    inline constexpr CRGB() : r(0), g(0), b(0) {}
    inline constexpr CRGB(uint8_t ir, uint8_t ig, uint8_t ib) : r(ir), g(ig), b(ib) {}
    inline constexpr CRGB(uint32_t colorcode)
        : r((colorcode >> 16) & 0xFF), g((colorcode >> 8) & 0xFF), b(colorcode & 0xFF) {}

    CRGB(const CHSV& hsv);

    inline uint8_t& operator[](uint8_t x) { return raw[x]; }
    inline const uint8_t& operator[](uint8_t x) const { return raw[x]; }

    inline CRGB& operator=(uint32_t colorcode) {
        r = (colorcode >> 16) & 0xFF;
        g = (colorcode >> 8) & 0xFF;
        b = colorcode & 0xFF;
        return *this;
    }
    CRGB& operator=(const CHSV& rhs);

    inline bool operator==(const CRGB& rhs) const {
        return (r == rhs.r) && (g == rhs.g) && (b == rhs.b);
    }
    inline bool operator!=(const CRGB& rhs) const {
        return !(*this == rhs);
    }
    inline explicit operator bool() const {
        return (r | g | b) != 0;
    }
    inline explicit operator uint32_t() const {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
    inline uint32_t as_uint32_t() const {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }

    inline CRGB& operator+=(const CRGB& rhs) {
        r = qadd8(r, rhs.r);
        g = qadd8(g, rhs.g);
        b = qadd8(b, rhs.b);
        return *this;
    }
    inline CRGB& operator-=(const CRGB& rhs) {
        r = qsub8(r, rhs.r);
        g = qsub8(g, rhs.g);
        b = qsub8(b, rhs.b);
        return *this;
    }
    inline CRGB& operator|=(const CRGB& rhs) {
        if (rhs.r > r) r = rhs.r;
        if (rhs.g > g) g = rhs.g;
        if (rhs.b > b) b = rhs.b;
        return *this;
    }
    inline CRGB& operator&=(const CRGB& rhs) {
        if (rhs.r < r) r = rhs.r;
        if (rhs.g < g) g = rhs.g;
        if (rhs.b < b) b = rhs.b;
        return *this;
    }

    inline CRGB& nscale8(uint8_t scale) {
        r = scale8(r, scale);
        g = scale8(g, scale);
        b = scale8(b, scale);
        return *this;
    }
    inline CRGB& nscale8_video(uint8_t scale) {
        r = scale8_video(r, scale);
        g = scale8_video(g, scale);
        b = scale8_video(b, scale);
        return *this;
    }
    inline CRGB& fadeToBlackBy(uint8_t step) {
        return nscale8(255 - step);
    }

    enum HTMLColorCode {
        AliceBlue = 0xF0F8FF,
        Amethyst = 0x9966CC,
        AntiqueWhite = 0xFAEBD7,
        Aqua = 0x00FFFF,
        Aquamarine = 0x7FFFD4,
        Azure = 0xF0FFFF,
        Beige = 0xF5F5DC,
        Bisque = 0xFFE4C4,
        Black = 0x000000,
        BlanchedAlmond = 0xFFEBCD,
        Blue = 0x0000FF,
        BlueViolet = 0x8A2BE2,
        Brown = 0xA52A2A,
        BurlyWood = 0xDEB887,
        CadetBlue = 0x5F9EA0,
        Chartreuse = 0x7FFF00,
        Chocolate = 0xD2691E,
        Coral = 0xFF7F50,
        CornflowerBlue = 0x6495ED,
        Cornsilk = 0xFFF8DC,
        Crimson = 0xDC143C,
        Cyan = 0x00FFFF,
        DarkBlue = 0x00008B,
        DarkCyan = 0x008B8B,
        DarkGoldenrod = 0xB8860B,
        DarkGray = 0xA9A9A9,
        DarkGrey = 0xA9A9A9,
        DarkGreen = 0x006400,
        DarkKhaki = 0xBDB76B,
        DarkMagenta = 0x8B008B,
        DarkOliveGreen = 0x556B2F,
        DarkOrange = 0xFF8C00,
        DarkOrchid = 0x9932CC,
        DarkRed = 0x8B0000,
        DarkSalmon = 0xE9967A,
        DarkSeaGreen = 0x8FBC8F,
        DarkSlateBlue = 0x483D8B,
        DarkSlateGray = 0x2F4F4F,
        DarkSlateGrey = 0x2F4F4F,
        DarkTurquoise = 0x00CED1,
        DarkViolet = 0x9400D3,
        DeepPink = 0xFF1493,
        DeepSkyBlue = 0x00BFFF,
        DimGray = 0x696969,
        DimGrey = 0x696969,
        DodgerBlue = 0x1E90FF,
        FireBrick = 0xB22222,
        FloralWhite = 0xFFFAF0,
        ForestGreen = 0x228B22,
        Fuchsia = 0xFF00FF,
        Gainsboro = 0xDCDCDC,
        GhostWhite = 0xF8F8FF,
        Gold = 0xFFD700,
        Goldenrod = 0xDAA520,
        Gray = 0x808080,
        Grey = 0x808080,
        Green = 0x008000,
        GreenYellow = 0xADFF2F,
        Honeydew = 0xF0FFF0,
        HotPink = 0xFF69B4,
        IndianRed = 0xCD5C5C,
        Indigo = 0x4B0082,
        Ivory = 0xFFFFF0,
        Khaki = 0xF0E68C,
        Lavender = 0xE6E6FA,
        LavenderBlush = 0xFFF0F5,
        LawnGreen = 0x7CFC00,
        LemonChiffon = 0xFFFACD,
        LightBlue = 0xADD8E6,
        LightCoral = 0xF08080,
        LightCyan = 0xE0FFFF,
        LightGoldenrodYellow = 0xFAFAD2,
        LightGreen = 0x90EE90,
        LightGrey = 0xD3D3D3,
        LightGray = 0xD3D3D3,
        LightPink = 0xFFB6C1,
        LightSalmon = 0xFFA07A,
        LightSeaGreen = 0x20B2AA,
        LightSkyBlue = 0x87CEFA,
        LightSlateGray = 0x778899,
        LightSlateGrey = 0x778899,
        LightSteelBlue = 0xB0C4DE,
        LightYellow = 0xFFFFE0,
        Lime = 0x00FF00,
        LimeGreen = 0x32CD32,
        Linen = 0xFAF0E6,
        Magenta = 0xFF00FF,
        Maroon = 0x800000,
        MediumAquamarine = 0x66CDAA,
        MediumBlue = 0x0000CD,
        MediumOrchid = 0xBA55D3,
        MediumPurple = 0x9370DB,
        MediumSeaGreen = 0x3CB371,
        MediumSlateBlue = 0x7B68EE,
        MediumSpringGreen = 0x00FA9A,
        MediumTurquoise = 0x48D1CC,
        MediumVioletRed = 0xC71585,
        MidnightBlue = 0x191970,
        MintCream = 0xF5FFFA,
        MistyRose = 0xFFE4E1,
        Moccasin = 0xFFE4B5,
        NavajoWhite = 0xFFDEAD,
        Navy = 0x000080,
        OldLace = 0xFDF5E6,
        Olive = 0x808000,
        OliveDrab = 0x6B8E23,
        Orange = 0xFFA500,
        OrangeRed = 0xFF4500,
        Orchid = 0xDA70D6,
        PaleGoldenrod = 0xEEE8AA,
        PaleGreen = 0x98FB98,
        PaleTurquoise = 0xAFEEEE,
        PaleVioletRed = 0xDB7093,
        PapayaWhip = 0xFFEFD5,
        PeachPuff = 0xFFDAB9,
        Peru = 0xCD853F,
        Pink = 0xFFC0CB,
        Plaid = 0xCC5533,
        Plum = 0xDDA0DD,
        PowderBlue = 0xB0E0E6,
        Purple = 0x800080,
        Red = 0xFF0000,
        RosyBrown = 0xBC8F8F,
        RoyalBlue = 0x4169E1,
        SaddleBrown = 0x8B4513,
        Salmon = 0xFA8072,
        SandyBrown = 0xF4A460,
        SeaGreen = 0x2E8B57,
        Seashell = 0xFFF5EE,
        Sienna = 0xA0522D,
        Silver = 0xC0C0C0,
        SkyBlue = 0x87CEEB,
        SlateBlue = 0x6A5ACD,
        SlateGray = 0x708090,
        SlateGrey = 0x708090,
        Snow = 0xFFFAFA,
        SpringGreen = 0x00FF7F,
        SteelBlue = 0x4682B4,
        Tan = 0xD2B48C,
        Teal = 0x008080,
        Thistle = 0xD8BFD8,
        Tomato = 0xFF6347,
        Turquoise = 0x40E0D0,
        Violet = 0xEE82EE,
        Wheat = 0xF5DEB3,
        White = 0xFFFFFF,
        WhiteSmoke = 0xF5F5F5,
        Yellow = 0xFFFF00,
        YellowGreen = 0x9ACD32
    };

    inline constexpr CRGB(HTMLColorCode colorcode) : CRGB((uint32_t)colorcode) {}
    inline CRGB& operator=(HTMLColorCode colorcode) { return operator=((uint32_t)colorcode); }
};

inline CRGB operator+(const CRGB& p1, const CRGB& p2) {
    CRGB ret(p1);
    ret += p2;
    return ret;
}

inline CRGB operator-(const CRGB& p1, const CRGB& p2) {
    CRGB ret(p1);
    ret -= p2;
    return ret;
}

struct __attribute__((packed)) CHSV {
    union {
        struct {
            uint8_t h;
            uint8_t s;
            uint8_t v;
        };
        struct {
            uint8_t hue;
            uint8_t sat;
            uint8_t val;
        };
        uint8_t raw[3];
    };

    inline constexpr CHSV() : h(0), s(0), v(0) {}
    inline constexpr CHSV(uint8_t ih, uint8_t is, uint8_t iv) : h(ih), s(is), v(iv) {}
};

void hsv2rgb_rainbow(const CHSV& hsv, CRGB& rgb);

inline CRGB::CRGB(const CHSV& hsv) {
    hsv2rgb_rainbow(hsv, *this);
}
inline CRGB& CRGB::operator=(const CHSV& rhs) {
    hsv2rgb_rainbow(rhs, *this);
    return *this;
}

// ============================================================================
// Palettes
// ============================================================================

struct CRGBPalette16 {
    CRGB entries[16];

    inline CRGBPalette16() {
        for (int i = 0; i < 16; i++) entries[i] = 0;
    }
    inline CRGBPalette16(const CRGB& c0, const CRGB& c1, const CRGB& c2, const CRGB& c3,
                         const CRGB& c4, const CRGB& c5, const CRGB& c6, const CRGB& c7,
                         const CRGB& c8, const CRGB& c9, const CRGB& c10, const CRGB& c11,
                         const CRGB& c12, const CRGB& c13, const CRGB& c14, const CRGB& c15) {
        entries[0] = c0; entries[1] = c1; entries[2] = c2; entries[3] = c3;
        entries[4] = c4; entries[5] = c5; entries[6] = c6; entries[7] = c7;
        entries[8] = c8; entries[9] = c9; entries[10] = c10; entries[11] = c11;
        entries[12] = c12; entries[13] = c13; entries[14] = c14; entries[15] = c15;
    }
    inline CRGB& operator[](uint8_t x) { return entries[x & 0x0F]; }
    inline const CRGB& operator[](uint8_t x) const { return entries[x & 0x0F]; }
};

typedef CRGBPalette16 TProgmemRGBPalette16;
#define FL_PROGMEM

extern const CRGBPalette16 PartyColors_p;
extern const CRGBPalette16 RainbowColors_p;

CRGB ColorFromPalette(const CRGBPalette16& pal, uint8_t index, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND);

// ============================================================================
// Buffer Operations
// ============================================================================

inline CRGB& nblend(CRGB& existing, const CRGB& overlay, fract8 amountOfOverlay) {
    existing.r = blend8(existing.r, overlay.r, amountOfOverlay);
    existing.g = blend8(existing.g, overlay.g, amountOfOverlay);
    existing.b = blend8(existing.b, overlay.b, amountOfOverlay);
    return existing;
}

inline CRGB blend(const CRGB& p1, const CRGB& p2, fract8 amountOfP2) {
    CRGB ret(p1);
    return nblend(ret, p2, amountOfP2);
}

inline void fadeToBlackBy(CRGB* leds, uint16_t num_leds, uint8_t step) {
    uint8_t scale = 255 - step;
    for (uint16_t i = 0; i < num_leds; i++) {
        leds[i].nscale8(scale);
    }
}

inline void fill_solid(CRGB* leds, uint16_t num_leds, const CRGB& color) {
    for (uint16_t i = 0; i < num_leds; i++) {
        leds[i] = color;
    }
}

inline void fill_rainbow(CRGB* leds, uint16_t num_leds, uint8_t initial_hue, uint8_t delta_hue = 5) {
    CHSV hsv(initial_hue, 255, 255);
    for (uint16_t i = 0; i < num_leds; i++) {
        hsv2rgb_rainbow(hsv, leds[i]);
        hsv.h += delta_hue;
    }
}

// ============================================================================
// TinyLED Controller & Hardware Output
// ============================================================================

class TinyLEDController {
private:
    uint8_t m_pin;
    uint16_t m_num_leds;
    CRGB* m_leds;
    EOrder m_order;
    uint8_t m_brightness;
    CRGB m_correction;
    uint8_t m_power_volts;
    uint32_t m_max_power_mA;
    bool m_initialized;

public:
    TinyLEDController(uint8_t pin = 0, uint16_t num_leds = 0, CRGB* leds = nullptr, EOrder order = GRB);

    void init(uint8_t pin, uint16_t num_leds, CRGB* leds, EOrder order = GRB);

    TinyLEDController& setCorrection(uint32_t corr);
    TinyLEDController& setCorrection(const CRGB& corr);
    void setBrightness(uint8_t b);
    uint8_t getBrightness() const { return m_brightness; }
    void setMaxPowerInVoltsAndMilliamps(uint8_t volts, uint32_t milliamps);
    void setMaxPower(uint8_t volts, uint32_t milliamps) { setMaxPowerInVoltsAndMilliamps(volts, milliamps); }

    void show();
    void clear(bool writeData = false);
    void clearData();

    // Support for external / swapped buffers
    void setLeds(CRGB* leds) { m_leds = leds; }
    CRGB* getLeds() { return m_leds; }
    uint16_t size() const { return m_num_leds; }
    uint8_t getPin() const { return m_pin; }
};

typedef TinyLEDController WS2812Controller;

// ============================================================================
// TinyLED Template API (Section 5 of TZ)
// ============================================================================

template<uint8_t _PIN, uint16_t _NUM_LEDS, EOrder _ORDER = GRB>
class TinyLED {
private:
    CRGB m_internal_buffer[_NUM_LEDS];
    CRGB* m_current_buffer;
    TinyLEDController m_controller;

public:
    // Default constructor: uses internal buffer
    TinyLED()
        : m_current_buffer(m_internal_buffer),
          m_controller(_PIN, _NUM_LEDS, m_internal_buffer, _ORDER) {}

    // Constructor with external buffer
    TinyLED(CRGB* external_buffer)
        : m_current_buffer(external_buffer ? external_buffer : m_internal_buffer),
          m_controller(_PIN, _NUM_LEDS, external_buffer ? external_buffer : m_internal_buffer, _ORDER) {}

    inline TinyLEDController& setCorrection(uint32_t corr) { return m_controller.setCorrection(corr); }
    inline TinyLEDController& setCorrection(const CRGB& corr) { return m_controller.setCorrection(corr); }
    inline void setBrightness(uint8_t b) { m_controller.setBrightness(b); }
    inline uint8_t getBrightness() const { return m_controller.getBrightness(); }
    inline void setMaxPower(uint8_t volts, uint32_t milliamps) { m_controller.setMaxPowerInVoltsAndMilliamps(volts, milliamps); }
    inline void setMaxPowerInVoltsAndMilliamps(uint8_t volts, uint32_t milliamps) { m_controller.setMaxPowerInVoltsAndMilliamps(volts, milliamps); }

    inline void show() { m_controller.show(); }
    inline void clear(bool writeData = false) { m_controller.clear(writeData); }
    inline void clearData() { m_controller.clearData(); }

    // Buffer access & swapping
    inline void setLeds(CRGB* external_buffer) {
        m_current_buffer = external_buffer ? external_buffer : m_internal_buffer;
        m_controller.setLeds(m_current_buffer);
    }
    inline void resetBuffer() { setLeds(m_internal_buffer); }

    inline CRGB& operator[](int index) { return m_current_buffer[index]; }
    inline const CRGB& operator[](int index) const { return m_current_buffer[index]; }
    inline CRGB* leds() { return m_current_buffer; }
    inline uint16_t size() const { return _NUM_LEDS; }
    inline TinyLEDController& controller() { return m_controller; }
};

// ============================================================================
// CFastLED Singleton / Compatibility API
// ============================================================================

class CFastLED {
private:
    TinyLEDController* m_primaryController;

public:
    CFastLED() : m_primaryController(nullptr) {}

    template<typename CHIPSET, uint8_t _DATA_PIN, EOrder _RGB_ORDER>
    TinyLEDController& addLeds(CRGB* data, uint16_t nLedsOrOffset) {
        static TinyLEDController controller(_DATA_PIN, nLedsOrOffset, data, _RGB_ORDER);
        controller.init(_DATA_PIN, nLedsOrOffset, data, _RGB_ORDER);
        m_primaryController = &controller;
        return controller;
    }

    template<typename CHIPSET, uint8_t _DATA_PIN>
    TinyLEDController& addLeds(CRGB* data, uint16_t nLedsOrOffset) {
        return addLeds<CHIPSET, _DATA_PIN, GRB>(data, nLedsOrOffset);
    }

    void show() {
        if (m_primaryController) m_primaryController->show();
    }
    void setBrightness(uint8_t scale) {
        if (m_primaryController) m_primaryController->setBrightness(scale);
    }
    uint8_t getBrightness() const {
        return m_primaryController ? m_primaryController->getBrightness() : 255;
    }
    void setMaxPowerInVoltsAndMilliamps(uint8_t volts, uint32_t milliamps) {
        if (m_primaryController) m_primaryController->setMaxPowerInVoltsAndMilliamps(volts, milliamps);
    }
    void setCorrection(uint32_t correction) {
        if (m_primaryController) m_primaryController->setCorrection(correction);
    }
    void setCorrection(const CRGB& correction) {
        if (m_primaryController) m_primaryController->setCorrection(correction);
    }
    void clear(bool writeData = false) {
        if (m_primaryController) m_primaryController->clear(writeData);
    }
    void clearData() {
        if (m_primaryController) m_primaryController->clearData();
    }

    uint16_t size() const { return m_primaryController ? m_primaryController->size() : 0; }
    uint16_t count() const { return size(); }
    CRGB* leds() { return m_primaryController ? m_primaryController->getLeds() : nullptr; }
};

typedef CFastLED CLEDController;
extern CFastLED FastLED;
