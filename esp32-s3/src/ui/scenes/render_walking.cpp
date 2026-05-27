#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_walking_footprints(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t groundY = SCREEN_H - 30;
    gfx.drawDashedLine(10, groundY, SCREEN_W - 10, groundY, colors.eye, 1, 3, 4);

    for (int i = 0; i < 6; i++) {
        float seq = fmodf(t * 1.0f + i * 0.16f, 1.0f);
        float x = lerp((float)(SCREEN_W + 24), -24.0f, seq);
        bool isLeft = (i % 2 == 0);
        int16_t dy = isLeft ? -6 : 6;
        float fadeIn = fminf(1.0f, seq * 4.0f);
        float fadeOut = fminf(1.0f, (1.0f - seq) * 4.0f);
        uint8_t op = (uint8_t)(fadeIn * fadeOut * 0.9f * 255.0f);

        gfx.fillEllipse((int16_t)x, (int16_t)(groundY + dy), 9, 6, colors.eye, op);
        gfx.fillCircle((int16_t)(x - 8), (int16_t)(groundY + dy - 8), 3, colors.eye, op);
        gfx.fillCircle((int16_t)(x - 4), (int16_t)(groundY + dy - 10), 3, colors.eye, op);
        gfx.fillCircle((int16_t)(x + 1), (int16_t)(groundY + dy - 10), 3, colors.eye, op);
        gfx.fillCircle((int16_t)(x + 6), (int16_t)(groundY + dy - 8), 3, colors.eye, op);
    }

    gfx.drawText("WALKING", SCX, STATUS_H + 22, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 140);
}

static void render_walking_runner(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t groundY = SCREEN_H - 30;
    float bob = fabsf(sinf(t * TAU * 4.0f)) * 4.0f;
    int16_t cx = SCX;
    int16_t cy = (int16_t)(groundY - 40.0f - bob);
    float armA = sinf(t * TAU * 4.0f) * 26.0f;
    float legA = sinf(t * TAU * 4.0f) * 22.0f;

    gfx.drawLine(10, groundY, SCREEN_W - 10, groundY, colors.eye, 2, 102);

    gfx.fillCircle(cx, (int16_t)(cy - 24), 10, colors.eye);
    gfx.drawLine(cx, (int16_t)(cy - 14), (int16_t)(cx + 2), (int16_t)(cy + 12),
                 colors.eye, 4);

    gfx.drawLine(cx, (int16_t)(cy - 8),
                 (int16_t)(cx - 12.0f + armA), (int16_t)(cy + 4.0f - armA / 2.0f),
                 colors.eye, 4);
    gfx.drawLine(cx, (int16_t)(cy - 8),
                 (int16_t)(cx + 12.0f - armA), (int16_t)(cy + 4.0f + armA / 2.0f),
                 colors.eye, 4);

    gfx.drawLine((int16_t)(cx + 2), (int16_t)(cy + 12),
                 (int16_t)(cx - 8.0f + legA), (int16_t)(groundY - bob),
                 colors.eye, 4);
    gfx.drawLine((int16_t)(cx + 2), (int16_t)(cy + 12),
                 (int16_t)(cx + 10.0f - legA), (int16_t)(groundY - bob),
                 colors.eye, 4);

    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 4.0f + i * 0.33f, 1.0f);
        gfx.drawLine((int16_t)(cx - 50.0f - p * 30.0f), (int16_t)(cy - 8 + i * 12),
                     (int16_t)(cx - 30.0f - p * 20.0f), (int16_t)(cy - 8 + i * 12),
                     colors.eye, 2, (uint8_t)((1.0f - p) * 178));
    }
}

const VariantDef WALKING_VARIANTS[] = {
    {"walking-footprints", "Footprints", 2400, TONE_GREEN, render_walking_footprints},
    {"walking-runner",     "Runner",     1200, TONE_RED,   render_walking_runner},
};

extern const CategoryDef CAT_WALKING = {
    "walking", "Walking", ContentKind::SCENE, TONE_GREEN,
    WALKING_VARIANTS, sizeof(WALKING_VARIANTS) / sizeof(WALKING_VARIANTS[0])
};
