#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>
#include <cstdio>

using namespace geom;
using namespace math;

static void render_timer_countdown(GfxEngine& gfx, float t, const ColorContext& colors) {
    int remaining = (int)fmaxf(0, ceilf(120.0f * (1.0f - t / 4.0f)));
    int mm = remaining / 60;
    int ss = remaining % 60;

    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);

    gfx.drawText(buf, SCX, CY + 16, colors.eye, 6, GfxEngine::TextAlign::CENTER);
    gfx.drawText("TIMER", SCX, STATUS_H + 22, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 153);
}

static void render_timer_progress(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX, cy = CY + 6;
    int16_t r = 64;

    gfx.drawArc(cx, cy, r, 0, TAU, colors.eye, 6, 51);

    float endAngle = -PI / 2.0f + t * TAU;
    gfx.drawArc(cx, cy, r, -PI / 2.0f, endAngle, colors.eye, 6);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", (int)((1.0f - t) * 100));
    gfx.drawText(buf, cx, cy + 8, colors.eye, 4, GfxEngine::TextAlign::CENTER);
}

static void render_timer_hourglass(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX, cy = CY + 4;
    float topFill = fmaxf(0.0f, 1.0f - t);
    float botFill = t;

    gfx.drawLine(cx - 28, cy - 50, cx + 28, cy - 50, colors.eye, 4);
    gfx.drawLine(cx - 28, cy + 50, cx + 28, cy + 50, colors.eye, 4);

    gfx.drawLine(cx - 28, cy - 50, cx + 4, cy, colors.eye, 4);
    gfx.drawLine(cx + 28, cy - 50, cx - 4, cy, colors.eye, 4);
    gfx.drawLine(cx - 4, cy, cx - 28, cy + 50, colors.eye, 4);
    gfx.drawLine(cx + 4, cy, cx + 28, cy + 50, colors.eye, 4);

    if (topFill > 0.05f) {
        int16_t topW = (int16_t)(24.0f * topFill);
        int16_t topBot = (int16_t)(cy - 50.0f * (1.0f - topFill));
        gfx.fillTriangle(cx - topW, cy - 50, cx + topW, cy - 50, cx + 4, topBot,
                         colors.eye);
        gfx.fillTriangle(cx - topW, cy - 50, cx - 4, topBot, cx + 4, topBot,
                         colors.eye);
    }

    if (botFill > 0.05f) {
        int16_t botW = (int16_t)(24.0f * botFill);
        int16_t botTop = (int16_t)(cy + 50.0f * (1.0f - botFill));
        gfx.fillTriangle(cx - botW, cy + 50, cx + botW, cy + 50, cx + 4, botTop,
                         colors.eye);
        gfx.fillTriangle(cx - botW, cy + 50, cx - 4, botTop, cx + 4, botTop,
                         colors.eye);
    }

    if (t > 0.02f && t < 0.98f) {
        float grainY = (float)cy + sinf(t * TAU * 4.0f) * 4.0f + 12.0f;
        gfx.fillCircle(cx, (int16_t)grainY, 2, colors.eye);
    }
}

const VariantDef TIMER_VARIANTS[] = {
    {"timer-countdown", "Countdown",     4000, TONE_ORANGE, render_timer_countdown},
    {"timer-progress",  "Ring progress", 4000, TONE_CYAN,   render_timer_progress},
    {"timer-hourglass", "Hourglass",     4000, TONE_WARM,   render_timer_hourglass},
};

extern const CategoryDef CAT_TIMER = {
    "timer", "Timer", ContentKind::SCENE, TONE_ORANGE,
    TIMER_VARIANTS, sizeof(TIMER_VARIANTS) / sizeof(TIMER_VARIANTS[0])
};
