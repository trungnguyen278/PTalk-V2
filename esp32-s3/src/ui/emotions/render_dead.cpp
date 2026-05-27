#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Draw X-shaped eye
static void drawXEye(GfxEngine& gfx, int16_t cx, int16_t cy, int16_t sz,
                     uint16_t color, uint8_t alpha) {
    gfx.drawLine(cx - sz, cy - sz, cx + sz, cy + sz, color, 10, alpha);
    gfx.drawLine(cx + sz, cy - sz, cx - sz, cy + sz, color, 10, alpha);
}

// dead-x: X eyes with flicker + frown
static void render_dead_x(GfxEngine& gfx, float t, const ColorContext& colors) {
    uint8_t alpha = (t > 0.92f) ? 77 : 255; // flicker at end
    drawXEye(gfx, LX, CY, 28, colors.eye, alpha);
    drawXEye(gfx, RX, CY, 28, colors.eye, alpha);

    // Frown arc
    int16_t mx = SCREEN_W / 2;
    gfx.drawQuadBezier(mx - 30, SCREEN_H - 32, mx, SCREEN_H - 32 + 8,
                       mx + 30, SCREEN_H - 32, colors.eye, 5);
}

// dead-fade: X eyes with slow fade-blink
static void render_dead_fade(GfxEngine& gfx, float t, const ColorContext& colors) {
    float op = 0.4f + fabsf(sinf(t * TAU * 0.5f)) * 0.6f;
    uint8_t alpha = (uint8_t)(op * 255.0f);
    drawXEye(gfx, LX, CY, 24, colors.eye, alpha);
    drawXEye(gfx, RX, CY, 24, colors.eye, alpha);
}

const VariantDef DEAD_VARIANTS[] = {
    {"dead-x",    "X eyes", 2800, TONE_NONE, render_dead_x},
    {"dead-fade", "Fade",   3600, TONE_NONE, render_dead_fade},
};

extern const CategoryDef CAT_DEAD = {
    "dead", "Dead", ContentKind::EMOTION, TONE_RED,
    DEAD_VARIANTS, sizeof(DEAD_VARIANTS) / sizeof(DEAD_VARIANTS[0])
};
