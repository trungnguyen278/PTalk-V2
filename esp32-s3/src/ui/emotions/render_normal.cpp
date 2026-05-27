#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// normal-steady: two eyes with periodic double-blink
static void render_normal_steady(GfxEngine& gfx, float t, const ColorContext& colors) {
    float b = fmaxf(blink(t, 0.32f, 0.07f), blink(t, 0.78f, 0.07f));
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
}

// normal-breathe: gentle scale + vertical bob
static void render_normal_breathe(GfxEngine& gfx, float t, const ColorContext& colors) {
    float s = 1.0f + sinf(t * TAU) * 0.05f;
    float dy = sinf(t * TAU) * 3.0f;
    float b = blink(t, 0.55f, 0.07f);
    int16_t h = (int16_t)lerp((float)EYE_H * s, 4.0f, b);
    int16_t w = (int16_t)(EYE_W * s);
    gfx.drawEye(LX, (int16_t)(CY + dy), w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + dy), w, h, EYE_RX, 0, colors.eye);
}

// normal-look: eyes with bg-colored "pupil" circles that move in a sequence
static void render_normal_look(GfxEngine& gfx, float t, const ColorContext& colors) {
    struct Pos { float x, y; };
    static constexpr Pos seq[] = {{0,0},{-8,0},{0,0},{8,0},{0,-4},{0,0}};
    constexpr int n = 6;

    int idx = clamp((float)((int)(t * n)), 0.0f, (float)(n - 1));
    float sub = t * n - idx;
    int nxt = (idx + 1) % n;
    float e = ease::inOut(sub);
    float dx = lerp(seq[idx].x, seq[nxt].x, e);
    float dy = lerp(seq[idx].y, seq[nxt].y, e);

    gfx.drawEyes(colors.eye);
    gfx.fillCircle((int16_t)(LX + dx), (int16_t)(CY + dy), 11, colors.bg);
    gfx.fillCircle((int16_t)(RX + dx), (int16_t)(CY + dy), 11, colors.bg);
}

// normal-hum: horizontal sway + tilt + blink
static void render_normal_hum(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sway = sinf(t * TAU) * 4.0f;
    float tilt = sinf(t * TAU) * 3.0f;
    float b = blink(t, 0.85f, 0.06f);
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye((int16_t)(LX + sway), CY, EYE_W, h, EYE_RX, tilt, colors.eye);
    gfx.drawEye((int16_t)(RX + sway), CY, EYE_W, h, EYE_RX, tilt, colors.eye);
}

// normal-twitch: micro-jump at t=0.62-0.66
static void render_normal_twitch(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t tw = (t > 0.62f && t < 0.66f) ? 4 : 0;
    float b = blink(t, 0.4f, 0.06f);
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye(LX + tw, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX + tw, CY, EYE_W, h, EYE_RX, 0, colors.eye);
}

// normal-tilt: whole face tilts side to side
static void render_normal_tilt(GfxEngine& gfx, float t, const ColorContext& colors) {
    float tilt = sinf(t * TAU) * 6.0f;
    float dy = cosf(t * TAU) * -2.0f;
    gfx.pushTransform();
    gfx.rotate(tilt, SCX, CY);
    gfx.drawEye(LX, (int16_t)(CY + dy), EYE_W, EYE_H, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + dy), EYE_W, EYE_H, EYE_RX, 0, colors.eye);
    gfx.popTransform();
}

// normal-double-blink: three rapid blinks
static void render_normal_double_blink(GfxEngine& gfx, float t, const ColorContext& colors) {
    float b = fmaxf(fmaxf(blink(t, 0.42f, 0.05f), blink(t, 0.52f, 0.05f)),
                    blink(t, 0.88f, 0.06f));
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
}

// normal-drift: slow diagonal floating
static void render_normal_drift(GfxEngine& gfx, float t, const ColorContext& colors) {
    float dx = sinf(t * TAU) * 5.0f;
    float dy = cosf(t * TAU * 0.5f) * 3.0f;
    float b = blink(t, 0.3f, 0.07f);
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye((int16_t)(LX + dx), (int16_t)(CY + dy), EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye((int16_t)(RX + dx), (int16_t)(CY + dy), EYE_W, h, EYE_RX, 0, colors.eye);
}

// --- Category registration ---
const VariantDef NORMAL_VARIANTS[] = {
    {"normal-steady",       "Steady",       5200, TONE_NONE, render_normal_steady},
    {"normal-breathe",      "Breathe",      3800, TONE_NONE, render_normal_breathe},
    {"normal-look",         "Look around",  4400, TONE_NONE, render_normal_look},
    {"normal-hum",          "Hum sway",     3000, TONE_NONE, render_normal_hum},
    {"normal-twitch",       "Micro twitch", 4800, TONE_NONE, render_normal_twitch},
    {"normal-tilt",         "Curious tilt", 3600, TONE_NONE, render_normal_tilt},
    {"normal-double-blink", "Double blink", 4400, TONE_NONE, render_normal_double_blink},
    {"normal-drift",        "Slow drift",   6000, TONE_NONE, render_normal_drift},
};

extern const CategoryDef CAT_NORMAL = {
    "normal", "Normal", ContentKind::EMOTION, TONE_CYAN,
    NORMAL_VARIANTS, sizeof(NORMAL_VARIANTS) / sizeof(NORMAL_VARIANTS[0])
};
