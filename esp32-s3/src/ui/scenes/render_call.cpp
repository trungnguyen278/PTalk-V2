#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>
#include <cstdio>

using namespace geom;
using namespace math;

static void render_call_incoming(GfxEngine& gfx, float t, const ColorContext& colors) {
    float wob = sinf(t * TAU * 6.0f) * 6.0f;
    int16_t cx = SCX - 56, cy = CY + 6;

    // Handset (simplified phone shape)
    gfx.pushTransform();
    gfx.translate((float)cx, (float)cy);
    gfx.rotate(wob, 0, 0);
    gfx.fillRoundedRect(-20, -22, 16, 18, 4, colors.eye);
    gfx.fillRoundedRect(4, 6, 18, 16, 4, colors.eye);
    gfx.drawLine(-8, -4, 8, 10, colors.eye, 6);
    gfx.popTransform();

    // Outgoing wave arcs
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 2.4f + i * 0.33f, 1.0f);
        int16_t r = (int16_t)(24.0f + p * 22.0f);
        uint8_t op = (uint8_t)((1.0f - p) * 178);
        gfx.drawArc((int16_t)(cx + 18), cy, r,
                    -0.6f, 0.6f, colors.eye, 3, op);
    }

    gfx.drawText("INCOMING", SCX + 50, cy - 8, colors.eye, 2,
                 GfxEngine::TextAlign::CENTER);
    gfx.drawText("CALL...", SCX + 50, cy + 16, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 191);
}

static void render_call_active(GfxEngine& gfx, float t, const ColorContext& colors) {
    int sec = (int)(t * 90.0f) + 12;
    int mm = sec / 60;
    int ss = sec % 60;
    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mm, ss);

    // Phone icon at top
    gfx.fillRoundedRect(SCX - 12, STATUS_H + 18, 10, 12, 3, colors.eye);
    gfx.fillRoundedRect(SCX + 2, STATUS_H + 30, 12, 10, 3, colors.eye);
    gfx.drawLine(SCX - 4, STATUS_H + 28, SCX + 6, STATUS_H + 34, colors.eye, 4);

    gfx.drawText(timeBuf, SCX, CY + 16, colors.eye, 5, GfxEngine::TextAlign::CENTER);

    // EQ bars at bottom
    for (int i = 0; i < 11; i++) {
        float phase = t * TAU * 3.0f + i * 0.7f;
        int16_t h = (int16_t)(4.0f + fabsf(sinf(phase)) * 18.0f);
        int16_t x = (int16_t)(40 + i * 24);
        gfx.fillRoundedRect(x - 3, SCREEN_H - 22 - h, 6, h, 2, colors.eye);
    }
}

static void render_call_missed(GfxEngine& gfx, float t, const ColorContext& colors) {
    uint8_t blinkO = ((int)(t * 3.0f) % 2 == 0) ? 255 : 128;

    // Phone icon
    gfx.fillRoundedRect(SCX - 20, CY - 12, 14, 16, 4, colors.eye);
    gfx.fillRoundedRect(SCX + 6, CY + 4, 16, 14, 4, colors.eye);
    gfx.drawLine(SCX - 8, CY + 2, SCX + 10, CY + 10, colors.eye, 5);

    // X mark
    gfx.drawLine(SCX + 11, CY - 25, SCX + 25, CY - 11, colors.eye, 4, blinkO);
    gfx.drawLine(SCX + 25, CY - 25, SCX + 11, CY - 11, colors.eye, 4, blinkO);

    gfx.drawText("MISSED CALL", SCX, SCREEN_H - 28, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 204);
}

const VariantDef CALL_VARIANTS[] = {
    {"call-incoming", "Incoming", 1400, TONE_GREEN, render_call_incoming},
    {"call-active",   "In call",  1500, TONE_CYAN,  render_call_active},
    {"call-missed",   "Missed",   2400, TONE_RED,   render_call_missed},
};

extern const CategoryDef CAT_CALL = {
    "call", "Call", ContentKind::SCENE, TONE_GREEN,
    CALL_VARIANTS, sizeof(CALL_VARIANTS) / sizeof(CALL_VARIANTS[0])
};
