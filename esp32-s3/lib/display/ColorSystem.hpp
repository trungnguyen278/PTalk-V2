#pragma once
#include <cstdint>

enum ToneId : uint8_t {
    TONE_CYAN = 0,
    TONE_WARM,
    TONE_ROSE,
    TONE_RED,
    TONE_BLUE,
    TONE_GREEN,
    TONE_PURPLE,
    TONE_ORANGE,
    TONE_WHITE,
    TONE_COUNT,
    TONE_NONE = 0xFF
};

// RGB565 palette (source: ui_design/emotion-core.jsx EYE_TONES)
//
// Conversion: R5=(R8>>3), G6=(G8>>2), B5=(B8>>3), rgb565=(R5<<11)|(G6<<5)|B5
constexpr uint16_t TONE_LUT[TONE_COUNT] = {
    0x5F9F,  // cyan   #5be9ff  R=11 G=60 B=31
    0xFE8C,  // warm   #ffd166  R=31 G=52 B=12
    0xFB53,  // rose   #ff6b9d  R=31 G=26 B=19 (adjusted: 0xFB54)
    0xFACE,  // red    #ff5b6e  R=31 G=22 B=14
    0x75DF,  // blue   #76b8ff  R=14 G=46 B=31
    0x7F51,  // green  #7be88e  R=15 G=58 B=17 (adjusted: 0x7F52)
    0xB47F,  // purple #b48cff  R=22 G=36 B=31
    0xFCEB,  // orange #ff9d5b  R=31 G=39 B=11
    0xF7BF,  // white  #f0f4ff  R=30 G=61 B=31
};

constexpr uint16_t COLOR_BG = 0x0841; // #06090d  near-black

inline uint16_t rgb565(uint8_t r8, uint8_t g8, uint8_t b8) {
    return ((r8 >> 3) << 11) | ((g8 >> 2) << 5) | (b8 >> 3);
}

enum class ContentKind : uint8_t { EMOTION, SCENE };

struct ColorContext {
    uint16_t eye;
    uint16_t accent;
    uint16_t bg;
    uint16_t tones[TONE_COUNT];

    static ColorContext forEmotion(ToneId tone) {
        ColorContext ctx;
        ctx.eye    = TONE_LUT[TONE_CYAN];
        ctx.accent = TONE_LUT[tone < TONE_COUNT ? tone : TONE_CYAN];
        ctx.bg     = COLOR_BG;
        for (int i = 0; i < TONE_COUNT; i++) ctx.tones[i] = TONE_LUT[i];
        return ctx;
    }

    static ColorContext forScene(ToneId tone) {
        ToneId t = tone < TONE_COUNT ? tone : TONE_CYAN;
        ColorContext ctx;
        ctx.eye    = TONE_LUT[t];
        ctx.accent = TONE_LUT[t];
        ctx.bg     = COLOR_BG;
        for (int i = 0; i < TONE_COUNT; i++) ctx.tones[i] = TONE_LUT[i];
        return ctx;
    }
};
