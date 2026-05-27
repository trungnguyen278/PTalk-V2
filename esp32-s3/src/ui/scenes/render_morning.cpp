#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_morning_sunrise(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t horizonY = SCREEN_H - 36;
    float rise = ease::out(clamp(t * 1.3f, 0.0f, 1.0f));
    int16_t sunY = (int16_t)lerp((float)(horizonY + 30), (float)(horizonY - 40), rise);

    gfx.drawLine(0, horizonY, SCREEN_W, horizonY, colors.eye, 2);

    // Sun rays
    gfx.pushTransform();
    gfx.translate((float)SCX, (float)sunY);
    for (int i = 0; i < 12; i++) {
        float a = (float)i / 12.0f * PI;
        float r1 = 40.0f;
        float r2 = 54.0f + sinf(t * TAU * 2.0f + (float)i) * 3.0f;
        gfx.drawLine((int16_t)(cosf(a) * r1), (int16_t)(-sinf(a) * r1),
                     (int16_t)(cosf(a) * r2), (int16_t)(-sinf(a) * r2),
                     colors.eye, 3, (uint8_t)(rise * 255));
    }
    gfx.popTransform();

    gfx.fillCircle(SCX, sunY, 28, colors.eye);

    // Below-horizon mask
    gfx.fillRect(0, horizonY + 1, SCREEN_W, SCREEN_H - horizonY, colors.bg);

    // Birds
    if (t > 0.4f) {
        for (int i = 0; i < 2; i++) {
            float p = fmodf(t * 0.3f + i * 0.5f, 1.0f);
            float bx = lerp(-10.0f, (float)(SCREEN_W + 10), p);
            uint8_t op = (uint8_t)((1.0f - p) * 230);
            float flap = sinf(t * TAU * 6.0f) * 0.3f + 1.0f;
            int16_t by = (int16_t)(STATUS_H + 30 + i * 18);
            gfx.pushAlpha(op);
            gfx.drawQuadBezier((int16_t)bx, by,
                               (int16_t)(bx + 6), (int16_t)(by - 8.0f * flap),
                               (int16_t)(bx + 12), by, colors.eye, 2);
            gfx.drawQuadBezier((int16_t)(bx + 12), by,
                               (int16_t)(bx + 18), (int16_t)(by - 8.0f * flap),
                               (int16_t)(bx + 24), by, colors.eye, 2);
            gfx.popAlpha();
        }
    }

    gfx.drawText("GOOD MORNING", SCX, STATUS_H + 22, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 191);
}

static void render_morning_alarm_rays(GfxEngine& gfx, float t, const ColorContext& colors) {
    float shake = sinf(t * TAU * 14.0f) * 3.0f;

    gfx.pushTransform();
    gfx.translate(shake, 0);

    // Bell body
    gfx.fillTriangle(SCX, CY + 6 - 36, SCX - 28, CY + 6 + 4,
                     SCX + 28, CY + 6 + 4, colors.eye);
    gfx.fillRoundedRect(SCX - 32, CY + 4, 64, 10, 4, colors.eye);
    // Clapper
    gfx.fillRoundedRect(SCX - 6, CY + 10, 12, 6, 2, colors.eye);
    // Top knobs
    gfx.fillRoundedRect(SCX - 14, CY + 6 - 44, 4, 10, 2, colors.eye);
    gfx.fillRoundedRect(SCX + 10, CY + 6 - 44, 4, 10, 2, colors.eye);

    // Rays
    for (int i = 0; i < 6; i++) {
        float a = -PI / 2.0f + ((float)i - 2.5f) * 0.25f;
        float r0 = 50.0f;
        float r1 = 60.0f + sinf(t * TAU * 4.0f + (float)i) * 4.0f;
        uint8_t op = (uint8_t)(fabsf(sinf(t * TAU * 4.0f + (float)i)) * 153 + 102);
        gfx.drawLine((int16_t)(SCX + cosf(a) * r0), (int16_t)(CY + 6 + sinf(a) * r0),
                     (int16_t)(SCX + cosf(a) * r1), (int16_t)(CY + 6 + sinf(a) * r1),
                     colors.eye, 3, op);
    }

    gfx.popTransform();

    gfx.drawText("WAKE UP", SCX, SCREEN_H - 20, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 178);
}

const VariantDef MORNING_VARIANTS[] = {
    {"morning-sunrise",    "Sunrise",    5000, TONE_WARM, render_morning_sunrise},
    {"morning-alarm-rays", "Wake alarm", 800,  TONE_RED,  render_morning_alarm_rays},
};

extern const CategoryDef CAT_MORNING = {
    "morning", "Morning", ContentKind::SCENE, TONE_WARM,
    MORNING_VARIANTS, sizeof(MORNING_VARIANTS) / sizeof(MORNING_VARIANTS[0])
};
