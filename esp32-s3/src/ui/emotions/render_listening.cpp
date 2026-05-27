#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// listening-attentive: wide eyes with breathing pupils + audio hint ring
static void render_listening_attentive(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t w = (int16_t)(EYE_W * 1.05f);
    int16_t h = (int16_t)(EYE_H * 1.05f);
    gfx.drawEye(LX, CY, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, w, h, EYE_RX, 0, colors.eye);

    float pulseR = 12.0f + sinf(t * TAU * 2.0f) * 1.5f;
    gfx.fillCircle(LX, CY, (int16_t)pulseR, colors.bg);
    gfx.fillCircle(RX, CY, (int16_t)pulseR, colors.bg);

    // Audio hint ring, bottom-right
    float ringR = 6.0f + sinf(t * TAU * 2.0f) * 3.0f;
    gfx.strokeCircle(SCREEN_W - 24, SCREEN_H - 40, (int16_t)ringR,
                     colors.accent, 2);
}

// listening-waves: eyes + audio bar visualizer at bottom
static void render_listening_waves(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t eyeY = CY - 18;
    int16_t eyeH = (int16_t)(EYE_H * 0.85f);
    gfx.drawEye(LX, eyeY, EYE_W, eyeH, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, eyeY, EYE_W, eyeH, EYE_RX, 0, colors.eye);

    constexpr int bars = 9;
    int16_t cx0 = SCREEN_W / 2 - ((bars - 1) * 14) / 2;
    for (int i = 0; i < bars; i++) {
        float phase = t * TAU * 2.0f + i * 0.6f;
        int16_t bh = (int16_t)(6.0f + fabsf(sinf(phase)) * 22.0f);
        int16_t bx = cx0 + i * 14 - 4;
        int16_t by = SCREEN_H - 22 - bh / 2;
        gfx.fillRoundedRect(bx, by, 8, bh, 3, colors.accent);
    }
}

// listening-eq: eye height pulses with envelope + side arcs
static void render_listening_eq(GfxEngine& gfx, float t, const ColorContext& colors) {
    float env = 0.5f + 0.5f * fabsf(sinf(t * TAU * 3.0f));
    int16_t h = (int16_t)(EYE_H * (0.7f + 0.25f * env));
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);

    // Side arcs (left and right)
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 2.0f + i * 0.3f, 1.0f);
        uint8_t op = (uint8_t)((1.0f - p) * 255.0f);

        // Left arcs
        int16_t lx = (int16_t)(18.0f + p * 12.0f);
        gfx.pushAlpha(op);
        gfx.drawQuadBezier(lx, CY - 16, lx - 6, CY, lx, CY + 16,
                           colors.accent, 3);
        gfx.popAlpha();

        // Right arcs
        int16_t rx = (int16_t)(SCREEN_W - 18.0f - p * 12.0f);
        gfx.pushAlpha(op);
        gfx.drawQuadBezier(rx, CY - 16, rx + 6, CY, rx, CY + 16,
                           colors.accent, 3);
        gfx.popAlpha();
    }
}

// listening-cup: eyes lean to one side + sound marks
static void render_listening_cup(GfxEngine& gfx, float t, const ColorContext& colors) {
    float lean = sinf(t * TAU * 0.6f) * 8.0f;
    int16_t h = (int16_t)(EYE_H * 0.9f);
    gfx.drawEye((int16_t)(LX + lean), CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye((int16_t)(RX + lean), CY, EYE_W, h, EYE_RX, 0, colors.eye);

    // Sound marks on right side
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 1.2f + i * 0.33f, 1.0f);
        int16_t x = (int16_t)(SCREEN_W - 12.0f - p * 14.0f);
        uint8_t op = (uint8_t)((1.0f - p) * 255.0f);
        gfx.pushAlpha(op);
        gfx.drawQuadBezier(x, CY - 14, x + 5, CY, x, CY + 14,
                           colors.accent, 3);
        gfx.popAlpha();
    }
}

// --- Category registration ---
const VariantDef LISTENING_VARIANTS[] = {
    {"listening-attentive", "Attentive",   2200, TONE_NONE, render_listening_attentive},
    {"listening-waves",     "Sound waves", 1600, TONE_NONE, render_listening_waves},
    {"listening-eq",        "EQ pulse",    1400, TONE_NONE, render_listening_eq},
    {"listening-cup",       "Cupped ear",  2600, TONE_NONE, render_listening_cup},
};

extern const CategoryDef CAT_LISTENING = {
    "listening", "Listening", ContentKind::EMOTION, TONE_CYAN,
    LISTENING_VARIANTS, sizeof(LISTENING_VARIANTS) / sizeof(LISTENING_VARIANTS[0])
};
