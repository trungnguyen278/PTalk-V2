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

static void render_night_moon(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX - 50, cy = CY - 8;

    gfx.fillCircle(cx, cy, 32, colors.eye);
    gfx.fillCircle((int16_t)(cx + 12), (int16_t)(cy - 4), 28, colors.bg);

    struct StarPos { int16_t x, y; float ph; };
    static const StarPos STARS[] = {
        {(int16_t)(SCX + 30), (int16_t)(STATUS_H + 24), 0.0f},
        {(int16_t)(SCX + 70), (int16_t)(STATUS_H + 48), 0.3f},
        {(int16_t)(SCX + 40), (int16_t)(CY + 28),       0.6f},
        {(int16_t)(SCX + 88), (int16_t)(CY - 8),        0.15f},
        {(int16_t)(SCX - 90), (int16_t)(STATUS_H + 44), 0.5f},
    };
    for (int i = 0; i < 5; i++) {
        float tw = 0.4f + fabsf(sinf((t + STARS[i].ph) * TAU)) * 0.6f;
        fillStar(gfx, STARS[i].x, STARS[i].y, 5.0f * tw, 2.0f, colors.eye,
                 (uint8_t)(tw * 255));
    }
}

static void render_night_stars(GfxEngine& gfx, float t, const ColorContext& colors) {
    for (int i = 0; i < 24; i++) {
        int16_t x = (int16_t)((i * 47 + 13) % (SCREEN_W - 16) + 8);
        int16_t y = (int16_t)(STATUS_H + ((i * 31 + 7) % (SCREEN_H - STATUS_H - 20)));
        float ph = (float)(i % 8) / 8.0f;
        float tw = 0.3f + fabsf(sinf((t + ph) * TAU)) * 0.7f;
        int16_t sz = (int16_t)(((i % 3) + 1) * 1.5f);
        gfx.fillCircle(x, y, sz, colors.eye, (uint8_t)(tw * 255));
    }

    gfx.drawText("GOOD NIGHT", SCX, SCREEN_H - 22, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 128);
}

const VariantDef NIGHT_VARIANTS[] = {
    {"night-moon",  "Moon & stars", 6000, TONE_PURPLE, render_night_moon},
    {"night-stars", "Starfield",    8000, TONE_WHITE,  render_night_stars},
};

extern const CategoryDef CAT_NIGHT = {
    "night", "Night", ContentKind::SCENE, TONE_PURPLE,
    NIGHT_VARIANTS, sizeof(NIGHT_VARIANTS) / sizeof(NIGHT_VARIANTS[0])
};
