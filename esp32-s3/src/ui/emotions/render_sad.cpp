#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Sad lid overlay: bg-colored quad that masks the upper part of the eye
// For left eye: outer (left) corner droops. For right eye: outer (right) droops.
static void drawSadLid(GfxEngine& gfx, int16_t cx, int16_t ey, int16_t slant,
                       bool isRight, uint16_t bg) {
    int16_t hw = EYE_W / 2 + 4;
    int16_t hh = EYE_H / 2 + 4;
    int16_t top = ey - hh;
    int16_t botFlat = ey - 6;
    int16_t botSlant = botFlat + slant;

    // 4 corners: TL, TR, BR, BL
    int16_t tlx = cx - hw, tly = top;
    int16_t trx = cx + hw, try_ = top;
    int16_t brx, bry, blx, bly;

    if (isRight) {
        blx = cx - hw; bly = botFlat;
        brx = cx + hw; bry = botSlant; // outer droops
    } else {
        blx = cx - hw; bly = botSlant; // outer droops
        brx = cx + hw; bry = botFlat;
    }

    // Two triangles to fill quad: TL-TR-BR and TL-BR-BL
    gfx.fillTriangle(tlx, tly, trx, try_, brx, bry, bg);
    gfx.fillTriangle(tlx, tly, brx, bry, blx, bly, bg);
}

// sad-droop: drooping eyes with sad lids + downturned mouth
static void render_sad_droop(GfxEngine& gfx, float t, const ColorContext& colors) {
    float droop = 4.0f + sinf(t * TAU * 0.5f) * 2.0f;
    int16_t y = (int16_t)(CY + droop);
    int16_t h = (int16_t)(EYE_H * 0.78f);

    gfx.drawEye(LX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    drawSadLid(gfx, LX, y, 30, false, colors.bg);
    drawSadLid(gfx, RX, y, 30, true, colors.bg);

    // Downturned mouth (arc curving upward = frown)
    int16_t mx = SCREEN_W / 2;
    int16_t my = SCREEN_H - 32;
    gfx.drawQuadBezier(mx - 18, my, mx, my + 6, mx + 18, my,
                       colors.eye, 4);
}

// sad-look-down: sad eyes looking down with pupil circles
static void render_sad_look_down(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t y = (int16_t)(CY + 8.0f + sinf(t * TAU * 0.5f) * 1.0f);
    int16_t h = (int16_t)(EYE_H * 0.7f);

    gfx.drawEye(LX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    drawSadLid(gfx, LX, y, 24, false, colors.bg);
    drawSadLid(gfx, RX, y, 24, true, colors.bg);
    // Looking-down pupils
    gfx.fillCircle(LX, y + 12, 8, colors.bg);
    gfx.fillCircle(RX, y + 12, 8, colors.bg);
}

// sad-big-droopy: large drooping eyes with highlight glints
static void render_sad_big_droopy(GfxEngine& gfx, float t, const ColorContext& colors) {
    float droop = 6.0f + sinf(t * TAU * 0.4f) * 3.0f;
    int16_t y = (int16_t)(CY + droop);
    int16_t w = (int16_t)(EYE_W * 1.05f);
    int16_t h = (int16_t)(EYE_H * 0.9f);

    gfx.drawEye(LX, y, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, w, h, EYE_RX, 0, colors.eye);
    drawSadLid(gfx, LX, y, 42, false, colors.bg);
    drawSadLid(gfx, RX, y, 42, true, colors.bg);
    // Highlight glints
    gfx.fillCircle(LX - 16, y + 6, 6, colors.bg);
    gfx.fillCircle(RX - 16, y + 6, 6, colors.bg);
}

// sad-quiver: trembling eyes + lower lip
static void render_sad_quiver(GfxEngine& gfx, float t, const ColorContext& colors) {
    float q = sinf(t * TAU * 6.0f) * 1.5f;
    int16_t y = CY + 4;
    int16_t h = (int16_t)(EYE_H * 0.75f);

    gfx.drawEye(LX, (int16_t)(y + q), EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(y - q), EYE_W, h, EYE_RX, 0, colors.eye);
    drawSadLid(gfx, LX, (int16_t)(y + q), 28, false, colors.bg);
    drawSadLid(gfx, RX, (int16_t)(y - q), 28, true, colors.bg);

    // Trembling lower lip
    int16_t mx = SCREEN_W / 2;
    int16_t my = SCREEN_H - 30;
    int16_t qd = (int16_t)(q * 2.0f);
    gfx.drawQuadBezier(mx - 22, my, mx, (int16_t)(my + qd), mx + 22, my,
                       colors.eye, 4);
}

// --- Category registration ---
const VariantDef SAD_VARIANTS[] = {
    {"sad-droop",      "Droop",        3200, TONE_NONE, render_sad_droop},
    {"sad-look-down",  "Looking down", 3400, TONE_NONE, render_sad_look_down},
    {"sad-big-droopy", "Big droopy",   4000, TONE_NONE, render_sad_big_droopy},
    {"sad-quiver",     "Quiver",       1600, TONE_NONE, render_sad_quiver},
};

extern const CategoryDef CAT_SAD = {
    "sad", "Sad", ContentKind::EMOTION, TONE_BLUE,
    SAD_VARIANTS, sizeof(SAD_VARIANTS) / sizeof(SAD_VARIANTS[0])
};
