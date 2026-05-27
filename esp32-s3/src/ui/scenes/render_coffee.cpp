#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_coffee_pour(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX, cy = CY + 22;
    bool streamAlive = t > 0.2f && t < 0.85f;
    float fill = clamp((t - 0.2f) / 0.6f, 0.0f, 1.0f);

    // Pot (simplified)
    gfx.strokeRoundedRect(cx - 60, cy - 60, 24, 24, 2, colors.eye, 4);
    gfx.drawLine(cx - 60, cy - 56, cx - 76, cy - 52, colors.eye, 5);
    gfx.drawLine(cx - 36, cy - 50, cx - 20, cy - 46, colors.eye, 4);

    // Stream
    if (streamAlive) {
        gfx.drawLine(cx - 18, cy - 42, cx - 4, cy - 4, colors.eye, 3);
    }

    // Cup outline
    gfx.drawQuadBezier(cx - 32, cy - 4, cx - 32, cy + 30, cx, cy + 30,
                       colors.eye, 4);
    gfx.drawQuadBezier(cx, cy + 30, cx + 32, cy + 30, cx + 32, cy - 4,
                       colors.eye, 4);
    gfx.drawLine(cx - 32, cy - 4, cx + 32, cy - 4, colors.eye, 4);
    // Handle
    gfx.drawQuadBezier(cx + 32, cy + 6, cx + 46, cy + 10, cx + 38, cy + 22,
                       colors.eye, 4);

    // Coffee filling (from bottom up)
    int16_t fillH = (int16_t)(28.0f * fill);
    if (fillH > 0) {
        gfx.fillRect(cx - 28, cy + 28 - fillH, 56, fillH, colors.eye);
    }

    // Steam after fill
    if (t > 0.7f) {
        uint8_t steamOp = (uint8_t)((t - 0.7f) * 3.3f * 255);
        for (int i = 0; i < 2; i++) {
            float p = fmodf(t * 0.8f + i * 0.5f, 1.0f);
            int16_t sx = cx - 8 + i * 16;
            int16_t sy = (int16_t)(cy - 14.0f - p * 30.0f);
            uint8_t op = (uint8_t)fminf((float)steamOp, (1.0f - p) * 217);
            gfx.pushAlpha(op);
            gfx.drawQuadBezier(sx, sy, (int16_t)(sx + 6), (int16_t)(sy - 8),
                               sx, (int16_t)(sy - 16), colors.eye, 3);
            gfx.popAlpha();
        }
    }
}

static void render_coffee_cup(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX, cy = CY + 18;

    // Steam plumes
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 0.7f + i * 0.33f, 1.0f);
        int16_t x = cx - 14 + i * 14;
        int16_t y = (int16_t)(cy - 30.0f - p * 60.0f);
        uint8_t op = (uint8_t)((1.0f - p) * 204);
        gfx.pushAlpha(op);
        gfx.drawQuadBezier(x, y, (int16_t)(x + 8), (int16_t)(y - 10),
                           x, (int16_t)(y - 20), colors.eye, 3);
        gfx.drawQuadBezier(x, (int16_t)(y - 20), (int16_t)(x - 8), (int16_t)(y - 30),
                           x, (int16_t)(y - 40), colors.eye, 3);
        gfx.popAlpha();
    }

    // Cup body
    gfx.fillRoundedRect(cx - 40, cy - 16, 80, 56, 8, colors.eye);
    // Rim cutout
    gfx.fillEllipse(cx, cy - 16, 40, 6, colors.bg);
    gfx.fillEllipse(cx, cy - 16, 32, 4, colors.eye);

    // Handle
    gfx.drawQuadBezier(cx + 40, cy - 6, cx + 58, cy, cx + 50, cy + 18,
                       colors.eye, 5);

    // Saucer
    gfx.fillEllipse(cx, cy + 46, 56, 8, colors.eye);
}

const VariantDef COFFEE_VARIANTS[] = {
    {"coffee-pour", "Pour",      2400, TONE_WARM, render_coffee_pour},
    {"coffee-cup",  "Cup steam", 2800, TONE_WARM, render_coffee_cup},
};

extern const CategoryDef CAT_COFFEE = {
    "coffee", "Coffee", ContentKind::SCENE, TONE_WARM,
    COFFEE_VARIANTS, sizeof(COFFEE_VARIANTS) / sizeof(COFFEE_VARIANTS[0])
};
