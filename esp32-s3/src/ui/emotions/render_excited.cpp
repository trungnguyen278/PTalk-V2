#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Simple 5-pointed star using triangles
static void fillStar(GfxEngine& gfx, int16_t cx, int16_t cy, float r1, float r2,
                     uint16_t color, uint8_t alpha = 255) {
    for (int i = 0; i < 5; i++) {
        float a1 = -1.5708f + i * (TAU / 5.0f); // -PI/2 start
        float a2 = a1 + TAU / 10.0f;
        float a3 = a1 + TAU / 5.0f;
        int16_t x0 = (int16_t)(cx + cosf(a1) * r1);
        int16_t y0 = (int16_t)(cy + sinf(a1) * r1);
        int16_t x1 = (int16_t)(cx + cosf(a2) * r2);
        int16_t y1 = (int16_t)(cy + sinf(a2) * r2);
        int16_t x2 = (int16_t)(cx + cosf(a3) * r1);
        int16_t y2 = (int16_t)(cy + sinf(a3) * r1);
        gfx.fillTriangle(x0, y0, x1, y1, cx, cy, color, alpha);
        gfx.fillTriangle(x1, y1, x2, y2, cx, cy, color, alpha);
    }
}

// excited-stars: star-shaped eyes with orbiting sparkle stars
static void render_excited_stars(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = fabsf(sinf(t * TAU * 2.0f)) * 8.0f;
    float scl = 1.0f + sinf(t * TAU * 4.0f) * 0.08f;
    float r1 = (float)(EYE_W / 2) * scl;
    float r2 = (float)(EYE_W / 4) * scl;
    fillStar(gfx, LX, (int16_t)(CY - bob), r1, r2, colors.eye);
    fillStar(gfx, RX, (int16_t)(CY - bob), r1, r2, colors.eye);

    for (int i = 0; i < 4; i++) {
        float p = fmodf(t * 2.0f + i * 0.25f, 1.0f);
        float a = i * (TAU / 4.0f) + t * TAU;
        float r = 80.0f + p * 40.0f;
        int16_t x = (int16_t)(SCREEN_W / 2 + cosf(a) * r);
        int16_t y = (int16_t)(CY + sinf(a) * r * 0.55f);
        float s = lerp(10.0f, 2.0f, p);
        uint8_t op = (uint8_t)((1.0f - p) * 255.0f);
        fillStar(gfx, x, y, s, s * 0.4f, colors.accent, op);
    }
}

// excited-bounce: bouncy eyes with squash/stretch
static void render_excited_bounce(GfxEngine& gfx, float t, const ColorContext& colors) {
    float b = fabsf(sinf(t * TAU * 2.0f));
    float dy = -14.0f * b;
    float sq = 1.0f - b * 0.15f;
    int16_t w = (int16_t)((float)EYE_W / sq);
    int16_t h = (int16_t)((float)EYE_H * sq);
    gfx.drawEye(LX, (int16_t)(CY + dy), w, h, 30, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + dy), w, h, 30, 0, colors.eye);
}

// excited-grin: wide eyes + big smile arc
static void render_excited_grin(GfxEngine& gfx, float t, const ColorContext& colors) {
    float wob = sinf(t * TAU * 3.0f) * 2.0f;
    int16_t w = (int16_t)(EYE_W * 1.1f);
    int16_t h = (int16_t)(EYE_H * 1.05f);
    int16_t y = (int16_t)(CY - 6 + wob);
    gfx.drawEye(LX, y, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, w, h, EYE_RX, 0, colors.eye);

    // Wide smile arc
    int16_t mx = SCREEN_W / 2;
    gfx.drawQuadBezier(mx - 60, SCREEN_H - 36, mx, SCREEN_H - 36 + 16,
                       mx + 60, SCREEN_H - 36, colors.eye, 6);
}

const VariantDef EXCITED_VARIANTS[] = {
    {"excited-stars",  "Star eyes", 1200, TONE_NONE, render_excited_stars},
    {"excited-bounce", "Bounce",     800, TONE_NONE, render_excited_bounce},
    {"excited-grin",   "Wide grin", 1600, TONE_NONE, render_excited_grin},
};

extern const CategoryDef CAT_EXCITED = {
    "excited", "Excited", ContentKind::EMOTION, TONE_WARM,
    EXCITED_VARIANTS, sizeof(EXCITED_VARIANTS) / sizeof(EXCITED_VARIANTS[0])
};
