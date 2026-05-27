#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>
#include <cstdio>

using namespace geom;
using namespace math;

static void fillHeart(GfxEngine& gfx, int16_t cx, int16_t cy, float s,
                      uint16_t color, uint8_t alpha = 255) {
    float k = s / 30.0f;
    int16_t r = (int16_t)(9.0f * k);
    int16_t bumpY = (int16_t)(cy - 9.0f * k);
    int16_t tipY = (int16_t)(cy + 9.0f * k);
    gfx.fillCircle((int16_t)(cx - r), bumpY, r, color, alpha);
    gfx.fillCircle((int16_t)(cx + r), bumpY, r, color, alpha);
    gfx.fillTriangle((int16_t)(cx - 2 * r), bumpY,
                     (int16_t)(cx + 2 * r), bumpY,
                     cx, tipY, color, alpha);
}

static void render_fitness_hr(GfxEngine& gfx, float t, const ColorContext& colors) {
    float beat = fmaxf(pulse(t, 0.15f, 0.06f), pulse(t, 0.35f, 0.06f));
    int bpm = 72 + (int)(sinf(t * TAU * 0.3f) * 4.0f);

    int16_t hx = SCX - 60, hy = CY - 4;
    float hs = 36.0f * (1.0f + beat * 0.18f);
    fillHeart(gfx, hx, hy, hs, colors.eye);

    char bpmBuf[8];
    snprintf(bpmBuf, sizeof(bpmBuf), "%d", bpm);
    gfx.drawText(bpmBuf, SCX + 50, hy + 4, colors.eye, 5, GfxEngine::TextAlign::CENTER);
    gfx.drawText("BPM", SCX + 50, hy + 30, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 166);

    // ECG line
    int16_t ecgY = CY + 44;
    int beats = 3;
    int16_t seg = (SCREEN_W - 28) / beats;
    for (int i = 0; i < beats; i++) {
        int16_t x0 = 14 + i * seg;
        gfx.drawLine(x0, ecgY, (int16_t)(x0 + seg * 0.30f), ecgY, colors.eye, 3);
        gfx.drawLine((int16_t)(x0 + seg * 0.30f), ecgY,
                     (int16_t)(x0 + seg * 0.35f), ecgY - 4, colors.eye, 3);
        gfx.drawLine((int16_t)(x0 + seg * 0.35f), ecgY - 4,
                     (int16_t)(x0 + seg * 0.40f), ecgY + 18, colors.eye, 3);
        gfx.drawLine((int16_t)(x0 + seg * 0.40f), ecgY + 18,
                     (int16_t)(x0 + seg * 0.45f), ecgY - 28, colors.eye, 3);
        gfx.drawLine((int16_t)(x0 + seg * 0.45f), ecgY - 28,
                     (int16_t)(x0 + seg * 0.50f), ecgY + 6, colors.eye, 3);
        gfx.drawLine((int16_t)(x0 + seg * 0.50f), ecgY + 6,
                     (int16_t)(x0 + seg * 0.55f), ecgY, colors.eye, 3);
        gfx.drawLine((int16_t)(x0 + seg * 0.55f), ecgY, (int16_t)(x0 + seg), ecgY,
                     colors.eye, 3);
    }

    // Sweep line
    int16_t sweepX = (int16_t)(14.0f + fmodf(t * 2.0f, 1.0f) * (SCREEN_W - 28));
    gfx.drawLine(sweepX, ecgY - 34, sweepX, ecgY + 24, colors.eye, 2, 102);
}

static void render_fitness_steps(GfxEngine& gfx, float t, const ColorContext& colors) {
    int steps = 6420 + (int)(t * 1200);
    int goal = 10000;
    float pct = (float)steps / (float)goal;

    // Footstep icons
    for (int i = 0; i < 4; i++) {
        float phase = fmodf(t * 3.0f - i * 0.18f + 1.0f, 1.0f);
        float opF = phase < 0.5f ? 1.0f : 1.0f - (phase - 0.5f) * 2.0f;
        uint8_t op = (uint8_t)(clamp(opF, 0.25f, 1.0f) * 255);
        int16_t fx = SCX - 90 + i * 60;
        int16_t fy = STATUS_H + 32;
        gfx.fillEllipse(fx, fy, 11, 7, colors.eye, op);
        gfx.fillCircle((int16_t)(fx - 9), (int16_t)(fy - 9), 3, colors.eye, op);
        gfx.fillCircle((int16_t)(fx - 4), (int16_t)(fy - 11), 3, colors.eye, op);
        gfx.fillCircle((int16_t)(fx + 2), (int16_t)(fy - 11), 3, colors.eye, op);
        gfx.fillCircle((int16_t)(fx + 8), (int16_t)(fy - 9), 3, colors.eye, op);
    }

    char stepBuf[12];
    snprintf(stepBuf, sizeof(stepBuf), "%d", steps);
    gfx.drawText(stepBuf, SCX, CY + 20, colors.eye, 5, GfxEngine::TextAlign::CENTER);

    char pctBuf[16];
    snprintf(pctBuf, sizeof(pctBuf), "STEPS  %d%%", (int)(pct * 100));
    gfx.drawText(pctBuf, SCX, CY + 46, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 166);

    // Progress bar
    int16_t barW = SCREEN_W - 56;
    gfx.strokeRoundedRect(28, SCREEN_H - 22, barW, 6, 3, colors.eye, 1);
    int16_t fw = (int16_t)((barW - 2) * clamp(pct, 0.0f, 1.0f));
    if (fw > 0) {
        gfx.fillRoundedRect(29, SCREEN_H - 21, fw, 4, 2, colors.eye);
    }
}

static void render_fitness_dumbbell(GfxEngine& gfx, float t, const ColorContext& colors) {
    float lift = fabsf(sinf(t * TAU)) * 26.0f;
    int16_t cy = (int16_t)(CY + 22.0f - lift);

    gfx.fillRoundedRect(SCX - 50, cy - 5, 100, 10, 3, colors.eye);
    gfx.fillRoundedRect(SCX - 60, cy - 14, 14, 28, 3, colors.eye);
    gfx.fillRoundedRect(SCX + 46, cy - 14, 14, 28, 3, colors.eye);
    gfx.fillRoundedRect(SCX - 68, cy - 18, 8, 36, 2, colors.eye);
    gfx.fillRoundedRect(SCX + 60, cy - 18, 8, 36, 2, colors.eye);

    gfx.drawText("WORKOUT", SCX, STATUS_H + 22, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 178);
}

const VariantDef FITNESS_VARIANTS[] = {
    {"fitness-hr",       "Heart rate",   1200, TONE_RED,   render_fitness_hr},
    {"fitness-steps",    "Step counter", 4000, TONE_GREEN, render_fitness_steps},
    {"fitness-dumbbell", "Dumbbell",     1400, TONE_WARM,  render_fitness_dumbbell},
};

extern const CategoryDef CAT_FITNESS = {
    "fitness", "Fitness", ContentKind::SCENE, TONE_RED,
    FITNESS_VARIANTS, sizeof(FITNESS_VARIANTS) / sizeof(FITNESS_VARIANTS[0])
};
