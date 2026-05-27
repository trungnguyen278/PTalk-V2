#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Draw a spiral shape using line segments
static void drawSpiral(GfxEngine& gfx, int16_t cx, int16_t cy, float rot,
                       uint16_t color) {
    constexpr float turns = 2.4f;
    constexpr int steps = 40;
    float rotRad = rot * (TAU / 360.0f);

    int16_t prevX = cx, prevY = cy;
    for (int i = 1; i <= steps; i++) {
        float frac = (float)i / steps;
        float a = frac * turns * TAU + rotRad;
        float r = frac * 32.0f;
        int16_t x = (int16_t)(cx + cosf(a) * r);
        int16_t y = (int16_t)(cy + sinf(a) * r);
        gfx.drawLine(prevX, prevY, x, y, color, 6);
        prevX = x;
        prevY = y;
    }
}

// dizzy-spirals: rotating spiral eyes
static void render_dizzy_spirals(GfxEngine& gfx, float t, const ColorContext& colors) {
    float rot = t * 360.0f * 2.0f;
    drawSpiral(gfx, LX, CY, rot, colors.eye);
    drawSpiral(gfx, RX, CY, -rot, colors.eye);
}

// dizzy-wobble: eyes wobble with rotation + sparkle
static void render_dizzy_wobble(GfxEngine& gfx, float t, const ColorContext& colors) {
    float wob = sinf(t * TAU * 3.0f) * 6.0f;
    float tilt = sinf(t * TAU * 3.0f + 0.5f) * 18.0f;
    int16_t h = (int16_t)(EYE_H * 0.7f);
    gfx.drawEye((int16_t)(LX + wob), CY, EYE_W, h, EYE_RX, tilt, colors.eye);
    gfx.drawEye((int16_t)(RX - wob), CY, EYE_W, h, EYE_RX, -tilt, colors.eye);

    // Sparkle cross near top-left
    int16_t sx = 30, sy = STATUS_H + 24;
    gfx.drawLine(sx, sy - 6, sx, sy + 6, colors.accent, 3);
    gfx.drawLine(sx - 6, sy, sx + 6, sy, colors.accent, 3);
}

const VariantDef DIZZY_VARIANTS[] = {
    {"dizzy-spirals", "Spiral eyes", 1600, TONE_NONE, render_dizzy_spirals},
    {"dizzy-wobble",  "Wobble",      1200, TONE_NONE, render_dizzy_wobble},
};

extern const CategoryDef CAT_DIZZY = {
    "dizzy", "Dizzy", ContentKind::EMOTION, TONE_PURPLE,
    DIZZY_VARIANTS, sizeof(DIZZY_VARIANTS) / sizeof(DIZZY_VARIANTS[0])
};
