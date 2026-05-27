#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// confused-qmark: asymmetric eye sizes + tilting question mark
static void render_confused_qmark(GfxEngine& gfx, float t, const ColorContext& colors) {
    bool swap = sinf(t * TAU) > 0.0f;
    int16_t lw = (int16_t)(swap ? EYE_W * 0.7f : EYE_W * 1.05f);
    int16_t lh = (int16_t)(swap ? EYE_H * 0.7f : EYE_H * 1.05f);
    int16_t rw = (int16_t)(swap ? EYE_W * 1.05f : EYE_W * 0.7f);
    int16_t rh = (int16_t)(swap ? EYE_H * 1.05f : EYE_H * 0.7f);
    gfx.drawEye(LX, CY, lw, lh, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, rw, rh, EYE_RX, 0, colors.eye);

    // Question mark (tilting)
    float tilt = sinf(t * TAU) * 14.0f;
    int16_t qx = SCREEN_W / 2 + 56;
    int16_t qy = STATUS_H + 18;
    gfx.pushTransform();
    gfx.rotate(tilt, (float)qx, (float)qy);
    // Question mark curve (simplified as strokes)
    gfx.drawQuadBezier(qx - 10, qy - 6, qx - 10, qy - 18, qx, qy - 18,
                       colors.accent, 5);
    gfx.drawQuadBezier(qx, qy - 18, qx + 10, qy - 18, qx + 10, qy - 8,
                       colors.accent, 5);
    gfx.drawQuadBezier(qx + 10, qy - 8, qx + 10, qy, qx, qy + 6,
                       colors.accent, 5);
    gfx.drawLine(qx, qy + 6, qx, qy + 10, colors.accent, 5);
    gfx.fillCircle(qx, qy + 20, 4, colors.accent);
    gfx.popTransform();
}

// confused-tilt: head tilts side to side with squished eyes
static void render_confused_tilt(GfxEngine& gfx, float t, const ColorContext& colors) {
    float tilt = sinf(t * TAU) * 12.0f;
    int16_t h = (int16_t)(EYE_H * 0.75f);

    gfx.pushTransform();
    gfx.rotate(tilt, (float)(SCREEN_W / 2), (float)CY);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);

    // Small confused mouth
    int16_t mx = SCREEN_W / 2;
    gfx.drawQuadBezier(mx - 15, SCREEN_H - 34, mx, SCREEN_H - 34 + 3,
                       mx + 15, SCREEN_H - 34, colors.eye, 4);
    gfx.popTransform();
}

const VariantDef CONFUSED_VARIANTS[] = {
    {"confused-qmark", "Question mark", 2600, TONE_NONE, render_confused_qmark},
    {"confused-tilt",  "Head tilt",     3000, TONE_NONE, render_confused_tilt},
};

extern const CategoryDef CAT_CONFUSED = {
    "confused", "Confused", ContentKind::EMOTION, TONE_ORANGE,
    CONFUSED_VARIANTS, sizeof(CONFUSED_VARIANTS) / sizeof(CONFUSED_VARIANTS[0])
};
