#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// bored-half: very narrow eyes that drift side to side + flat mouth
static void render_bored_half(GfxEngine& gfx, float t, const ColorContext& colors) {
    float look = sinf(t * TAU) * 10.0f;
    int16_t h = (int16_t)(EYE_H * 0.35f);
    gfx.drawEye((int16_t)(LX + look), CY + 14, EYE_W, h, 14, 0, colors.eye);
    gfx.drawEye((int16_t)(RX + look), CY + 14, EYE_W, h, 14, 0, colors.eye);

    // Flat mouth line
    gfx.fillRoundedRect(SCREEN_W / 2 - 20, SCREEN_H - 30, 40, 4, 2,
                        colors.eye, 153); // 0.6 opacity
}

// bored-sideways: half-height eyes with pupils looking to one side
static void render_bored_sideways(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.6f);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);

    int16_t slide = sinf(t * TAU) > 0.0f ? 18 : -18;
    gfx.fillCircle(LX + slide, CY, 14, colors.bg);
    gfx.fillCircle(RX + slide, CY, 14, colors.bg);
}

const VariantDef BORED_VARIANTS[] = {
    {"bored-half",     "Half lids",      4200, TONE_NONE, render_bored_half},
    {"bored-sideways", "Sideways stare", 3800, TONE_NONE, render_bored_sideways},
};

extern const CategoryDef CAT_BORED = {
    "bored", "Bored", ContentKind::EMOTION, TONE_CYAN,
    BORED_VARIANTS, sizeof(BORED_VARIANTS) / sizeof(BORED_VARIANTS[0])
};
