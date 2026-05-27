#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void fillStar(GfxEngine& gfx, int16_t cx, int16_t cy, float r1, float r2,
                     uint16_t color, uint8_t alpha = 255) {
    for (int i = 0; i < 5; i++) {
        float a1 = -1.5708f + i * (TAU / 5.0f);
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

static void render_gaming_pad(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX, cy = CY + 8;
    int dpadDir = ((int)(t * 4.0f)) % 4;
    bool aPressed = fmodf(t * 4.0f, 1.0f) > 0.5f;

    // Controller body
    gfx.fillRoundedRect(cx - 88, cy - 22, 176, 48, 24, colors.eye);
    gfx.strokeRoundedRect(cx - 88, cy - 22, 176, 48, 24, colors.bg, 3);

    // D-pad (bg-colored cross)
    int16_t dx = cx - 58, dy = cy;
    gfx.fillRoundedRect(dx - 18, dy - 6, 36, 12, 2, colors.bg);
    gfx.fillRoundedRect(dx - 6, dy - 18, 12, 36, 2, colors.bg);

    // Active direction highlight
    if (dpadDir == 0) gfx.fillRect(dx - 6, dy - 18, 12, 12, colors.eye);
    if (dpadDir == 1) gfx.fillRect(dx + 6, dy - 6, 12, 12, colors.eye);
    if (dpadDir == 2) gfx.fillRect(dx - 6, dy + 6, 12, 12, colors.eye);
    if (dpadDir == 3) gfx.fillRect(dx - 18, dy - 6, 12, 12, colors.eye);

    // A/B buttons
    int16_t bx = cx + 56;
    gfx.fillCircle(bx - 12, cy + 6, 10, colors.bg);
    gfx.fillCircle(bx + 14, cy - 6, 10, aPressed ? colors.eye : colors.bg);
    gfx.drawText("B", bx - 12, cy + 10, colors.eye, 1, GfxEngine::TextAlign::CENTER);
    gfx.drawText("A", bx + 14, cy - 2, aPressed ? colors.bg : colors.eye, 1,
                 GfxEngine::TextAlign::CENTER);

    gfx.drawText("PLAY", SCX, STATUS_H + 22, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 178);
}

static void render_gaming_powerup(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sc = lerp(0.5f, 1.2f, ease::out(clamp(t * 1.5f, 0.0f, 1.0f)));
    int16_t cx = SCX, cy = CY + 6;

    gfx.pushTransform();
    gfx.translate((float)cx, (float)cy);
    gfx.scale(sc, sc);

    fillStar(gfx, 0, 0, 44, 18, colors.eye);
    fillStar(gfx, 0, 0, 26, 10, colors.bg);
    fillStar(gfx, 0, 0, 12, 4, colors.eye);

    // Orbiting sparkles (inside scale transform)
    for (int i = 0; i < 4; i++) {
        float a = (float)i / 4.0f * TAU + t * TAU * 2.0f;
        float r = 60.0f;
        fillStar(gfx,
                 (int16_t)(cosf(a) * r),
                 (int16_t)(sinf(a) * r),
                 6.0f, 2.5f, colors.eye, 217);
    }

    gfx.popTransform();
}

const VariantDef GAMING_VARIANTS[] = {
    {"gaming-pad",     "Game pad", 1600, TONE_PURPLE, render_gaming_pad},
    {"gaming-powerup", "Power up", 1800, TONE_WARM,   render_gaming_powerup},
};

extern const CategoryDef CAT_GAMING = {
    "gaming", "Gaming", ContentKind::SCENE, TONE_PURPLE,
    GAMING_VARIANTS, sizeof(GAMING_VARIANTS) / sizeof(GAMING_VARIANTS[0])
};
