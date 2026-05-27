#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>
#include <cstdio>

using namespace geom;
using namespace math;

static void render_clock_digital(GfxEngine& gfx, float t, const ColorContext& colors) {
    int hh = 12;
    int mm = (int)(t * 60.0f) % 60;
    uint8_t colonOp = ((int)(t * 2.0f) % 2 == 0) ? 255 : 51;

    char hBuf[4], mBuf[4];
    snprintf(hBuf, sizeof(hBuf), "%02d", hh);
    snprintf(mBuf, sizeof(mBuf), "%02d", mm);

    gfx.drawText(hBuf, SCX - 56, CY + 12, colors.eye, 6, GfxEngine::TextAlign::CENTER);
    gfx.drawText(":", SCX, CY + 8, colors.eye, 5, GfxEngine::TextAlign::CENTER, colonOp);
    gfx.drawText(mBuf, SCX + 56, CY + 12, colors.eye, 6, GfxEngine::TextAlign::CENTER);
}

static void render_clock_analog(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX, cy = CY + 4;
    int16_t r = 76;

    float sec = t * 60.0f;
    float mn = t * 60.0f;
    float hr = t * 12.0f;

    gfx.strokeCircle(cx, cy, r, colors.eye, 3);

    for (int i = 0; i < 12; i++) {
        float a = (float)i / 12.0f * TAU - PI / 2.0f;
        int16_t inner = (i % 3 == 0) ? r - 14 : r - 8;
        gfx.drawLine((int16_t)(cx + cosf(a) * inner), (int16_t)(cy + sinf(a) * inner),
                     (int16_t)(cx + cosf(a) * (r - 2)), (int16_t)(cy + sinf(a) * (r - 2)),
                     colors.eye, (i % 3 == 0) ? 3 : 2);
    }

    float hAngle = hr / 12.0f * TAU - PI / 2.0f;
    gfx.drawLine(cx, cy,
                 (int16_t)(cx + cosf(hAngle) * 38),
                 (int16_t)(cy + sinf(hAngle) * 38),
                 colors.eye, 6);

    float mAngle = mn / 60.0f * TAU - PI / 2.0f;
    gfx.drawLine(cx, cy,
                 (int16_t)(cx + cosf(mAngle) * 58),
                 (int16_t)(cy + sinf(mAngle) * 58),
                 colors.eye, 4);

    float sAngle = sec / 60.0f * TAU - PI / 2.0f;
    gfx.drawLine(cx, cy,
                 (int16_t)(cx + cosf(sAngle) * 64),
                 (int16_t)(cy + sinf(sAngle) * 64),
                 colors.eye, 2, 217);

    gfx.fillCircle(cx, cy, 4, colors.eye);
}

static void render_clock_alarm(GfxEngine& gfx, float t, const ColorContext& colors) {
    float shake = sinf(t * TAU * 12.0f) * 4.0f;
    int16_t cx = SCX, cy = CY + 8;

    gfx.pushTransform();
    gfx.translate(shake, 0);

    gfx.fillTriangle(cx, cy - 50, cx - 40, cy + 16, cx + 40, cy + 16,
                     colors.eye);
    gfx.fillRoundedRect(cx - 42, cy + 10, 84, 14, 7, colors.eye);
    gfx.fillCircle(cx, cy - 50, 6, colors.eye);
    gfx.fillCircle(cx, cy + 32, 6, colors.eye);

    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 3.0f + i * 0.33f, 1.0f);
        uint8_t op = (uint8_t)((1.0f - p) * 178);
        int16_t dr = (int16_t)(24.0f + p * 14.0f);
        gfx.drawArc((int16_t)(cx - 50), cy + 4, dr,
                    -0.5f, 0.5f, colors.eye, 3, op);
        gfx.drawArc((int16_t)(cx + 50), cy + 4, dr,
                    PI - 0.5f, PI + 0.5f, colors.eye, 3, op);
    }

    gfx.popTransform();
}

const VariantDef CLOCK_VARIANTS[] = {
    {"clock-digital", "Digital",    60000, TONE_CYAN, render_clock_digital},
    {"clock-analog",  "Analog",     60000, TONE_CYAN, render_clock_analog},
    {"clock-alarm",   "Alarm ring", 800,   TONE_RED,  render_clock_alarm},
};

extern const CategoryDef CAT_CLOCK = {
    "clock", "Clock", ContentKind::SCENE, TONE_CYAN,
    CLOCK_VARIANTS, sizeof(CLOCK_VARIANTS) / sizeof(CLOCK_VARIANTS[0])
};
