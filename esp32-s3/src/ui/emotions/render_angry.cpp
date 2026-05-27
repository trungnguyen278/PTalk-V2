#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Shared angry brow lines
static void drawAngryBrows(GfxEngine& gfx, float t, uint16_t color) {
    float sh = sinf(t * TAU * 6.0f) * 1.0f;
    // Left brow: outer-high to inner-low (V shape)
    gfx.drawLine(LX - 38, (int16_t)(CY - 50 + sh), LX + 32, CY - 28, color, 10);
    // Right brow: outer-high to inner-low
    gfx.drawLine(RX + 38, (int16_t)(CY - 50 - sh), RX - 32, CY - 28, color, 10);
}

// angry-glare: squinted eyes with angry brows
static void render_angry_glare(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sh = sinf(t * TAU * 4.0f) * 0.8f;
    int16_t h = (int16_t)(EYE_H * 0.55f);
    gfx.drawEye((int16_t)(LX + sh), CY, EYE_W, h, 12, 0, colors.eye);
    gfx.drawEye((int16_t)(RX - sh), CY, EYE_W, h, 12, 0, colors.eye);
    drawAngryBrows(gfx, t, colors.eye);
}

// angry-steam: squinted eyes + brows + rising steam particles
static void render_angry_steam(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.5f);
    gfx.drawEye(LX, CY, EYE_W, h, 10, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, 10, 0, colors.eye);
    drawAngryBrows(gfx, t, colors.eye);

    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 1.5f + i * 0.33f, 1.0f);
        int16_t x;
        if (i % 2 == 0) x = 28;
        else x = SCREEN_W - 28;
        if (i == 1) x += (int16_t)(sinf(p * TAU) * 6.0f);
        int16_t y = (int16_t)(CY - 30 - p * 60);
        int16_t r = (int16_t)(4.0f + p * 4.0f);
        uint8_t op = (uint8_t)((1.0f - p) * 0.7f * 255.0f);
        gfx.fillCircle(x, y, r, colors.accent, op);
    }
}

// angry-shake: whole face shakes rapidly
static void render_angry_shake(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sh = sinf(t * TAU * 8.0f) * 4.0f;
    float sv = cosf(t * TAU * 8.0f) * 2.0f;
    int16_t h = (int16_t)(EYE_H * 0.55f);

    gfx.pushTransform();
    gfx.translate(sh, sv);
    gfx.drawEye(LX, CY, EYE_W, h, 14, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, 14, 0, colors.eye);
    drawAngryBrows(gfx, t * 4.0f, colors.eye);
    gfx.popTransform();
}

// --- Category registration ---
const VariantDef ANGRY_VARIANTS[] = {
    {"angry-glare", "Glare",         1800, TONE_NONE, render_angry_glare},
    {"angry-steam", "Steam",         2200, TONE_NONE, render_angry_steam},
    {"angry-shake", "Furious shake",  700, TONE_NONE, render_angry_shake},
};

extern const CategoryDef CAT_ANGRY = {
    "angry", "Angry", ContentKind::EMOTION, TONE_RED,
    ANGRY_VARIANTS, sizeof(ANGRY_VARIANTS) / sizeof(ANGRY_VARIANTS[0])
};
