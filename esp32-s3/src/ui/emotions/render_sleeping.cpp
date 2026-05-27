#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// sleeping-zzz: closed arc eyes + drifting "z" characters
static void render_sleeping_zzz(GfxEngine& gfx, float t, const ColorContext& colors) {
    // Closed eyes (smile arcs, curving downward)
    int16_t hw = EYE_W / 2;
    int16_t cy = CY + 14;
    gfx.drawQuadBezier(LX - hw, cy, LX, cy + 14, LX + hw, cy, colors.eye, 10);
    gfx.drawQuadBezier(RX - hw, cy, RX, cy + 14, RX + hw, cy, colors.eye, 10);

    // Drifting "z" letters
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t + i * 0.33f, 1.0f);
        uint8_t scale = (uint8_t)(1 + p * 2);
        int16_t x = (int16_t)(RX + 22 + i * 6 + p * 24);
        int16_t y = (int16_t)(CY - 24 - p * 36);
        uint8_t op = (uint8_t)((1.0f - p) * 255.0f);
        gfx.drawChar(x, y, 'z', colors.accent, scale, op);
    }
}

// sleeping-calm: gentle breathing closed eyes
static void render_sleeping_calm(GfxEngine& gfx, float t, const ColorContext& colors) {
    float breathe = sinf(t * TAU) * 1.5f;
    int16_t hw = (EYE_W - 6) / 2;
    int16_t cy = (int16_t)(CY + 16 + breathe);
    gfx.drawQuadBezier(LX - hw, cy, LX, cy + 12, LX + hw, cy, colors.eye, 9);
    gfx.drawQuadBezier(RX - hw, cy, RX, cy + 12, RX + hw, cy, colors.eye, 9);
}

// --- Category registration ---
const VariantDef SLEEPING_VARIANTS[] = {
    {"sleeping-zzz",  "Zzz drift", 3200, TONE_NONE, render_sleeping_zzz},
    {"sleeping-calm", "Calm",      4400, TONE_NONE, render_sleeping_calm},
};

extern const CategoryDef CAT_SLEEPING = {
    "sleeping", "Sleeping", ContentKind::EMOTION, TONE_PURPLE,
    SLEEPING_VARIANTS, sizeof(SLEEPING_VARIANTS) / sizeof(SLEEPING_VARIANTS[0])
};
