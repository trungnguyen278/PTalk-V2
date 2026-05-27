#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_cooking_pan(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX, cy = CY + 24;

    gfx.drawLine(cx - 70, cy, cx + 70, cy, colors.eye, 5);
    gfx.drawQuadBezier(cx - 70, cy, cx - 70, cy + 32, cx - 56, cy + 32,
                       colors.eye, 5);
    gfx.drawLine(cx - 56, cy + 32, cx + 56, cy + 32, colors.eye, 5);
    gfx.drawQuadBezier(cx + 70, cy, cx + 70, cy + 32, cx + 56, cy + 32,
                       colors.eye, 5);
    gfx.drawLine(cx + 70, cy + 6, cx + 108, cy - 6, colors.eye, 6);

    for (int i = 0; i < 4; i++) {
        float p = fmodf(t * 2.0f + i * 0.25f, 1.0f);
        int16_t x = cx - 36 + i * 24;
        int16_t y = (int16_t)(cy + 16.0f - p * 8.0f);
        int16_t r = (int16_t)(4.0f - p * 2.0f);
        gfx.fillCircle(x, y, r, colors.eye, (uint8_t)((1.0f - p) * 255));
    }

    for (int i = -1; i <= 1; i++) {
        float flicker = sinf(t * TAU * (5.0f + i) + (float)i) * 4.0f;
        int16_t fx = (int16_t)(cx + i * 18);
        int16_t fy = cy + 50;
        gfx.fillTriangle(fx - 8, fy, fx + 8, fy,
                         fx, (int16_t)(fy - 20.0f - flicker),
                         colors.eye, 217);
    }

    gfx.drawText("COOKING", SCX, STATUS_H + 20, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 153);
}

static void render_cooking_pot(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX, cy = CY + 32;

    gfx.strokeRoundedRect(cx - 50, cy - 16, 100, 40, 4, colors.eye, 5);
    gfx.drawLine(cx - 60, cy - 16, cx + 60, cy - 16, colors.eye, 6);
    gfx.drawLine(cx - 60, cy - 16, cx - 66, cy - 10, colors.eye, 5);
    gfx.drawLine(cx + 60, cy - 16, cx + 66, cy - 10, colors.eye, 5);

    for (int i = 0; i < 5; i++) {
        float p = fmodf(t * 1.5f + i * 0.2f, 1.0f);
        int16_t x = cx - 32 + i * 16;
        int16_t y = (int16_t)(cy + 16.0f - p * 22.0f);
        int16_t r = (int16_t)(3.0f + (1.0f - p) * 2.0f);
        gfx.fillCircle(x, y, r, colors.eye, (uint8_t)((1.0f - p) * 255));
    }

    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 0.8f + i * 0.33f, 1.0f);
        int16_t x = cx - 24 + i * 24;
        int16_t y = (int16_t)(cy - 26.0f - p * 50.0f);
        uint8_t op = (uint8_t)((1.0f - p) * 204);
        gfx.pushAlpha(op);
        gfx.drawQuadBezier(x, y, (int16_t)(x + 6), (int16_t)(y - 8),
                           x, (int16_t)(y - 16), colors.eye, 3);
        gfx.drawQuadBezier(x, (int16_t)(y - 16), (int16_t)(x - 6), (int16_t)(y - 24),
                           x, (int16_t)(y - 32), colors.eye, 3);
        gfx.popAlpha();
    }
}

const VariantDef COOKING_VARIANTS[] = {
    {"cooking-pan", "Sizzling pan", 1600, TONE_RED,  render_cooking_pan},
    {"cooking-pot", "Boiling pot",  2400, TONE_BLUE, render_cooking_pot},
};

extern const CategoryDef CAT_COOKING = {
    "cooking", "Cooking", ContentKind::SCENE, TONE_ORANGE,
    COOKING_VARIANTS, sizeof(COOKING_VARIANTS) / sizeof(COOKING_VARIANTS[0])
};
