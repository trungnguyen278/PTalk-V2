#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void fillHeart(GfxEngine& gfx, int16_t cx, int16_t cy, float s,
                      uint16_t color, uint8_t alpha = 255) {
    float k = s / 30.0f;
    int16_t r = (int16_t)(9.0f * k);
    int16_t bumpY = (int16_t)(cy - 9.0f * k);
    int16_t tipY = (int16_t)(cy + 9.0f * k);
    gfx.fillCircle((int16_t)(cx - r), bumpY, r, color, alpha);
    gfx.fillCircle((int16_t)(cx + r), bumpY, r, color, alpha);
    gfx.fillTriangle((int16_t)(cx - 2 * r), bumpY,
                     (int16_t)(cx + 2 * r), bumpY,
                     cx, tipY, color, alpha);
}

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

static void render_gift_wrapped(GfxEngine& gfx, float t, const ColorContext& colors) {
    float wob = sinf(t * TAU * 3.0f) * 2.0f;
    int16_t cx = (int16_t)(SCX + wob), cy = CY + 8;

    gfx.pushTransform();
    gfx.translate(wob, 0);

    // Box body
    gfx.fillRoundedRect(cx - 50, cy - 30, 100, 70, 4, colors.eye);
    // Cross ribbons (bg color)
    gfx.fillRect(cx - 6, cy - 30, 12, 70, colors.bg);
    gfx.fillRect(cx - 50, cy - 6, 100, 6, colors.bg);
    // Ribbon overlay (thinner, eye color)
    gfx.fillRect(cx - 3, cy - 30, 6, 70, colors.eye);
    gfx.fillRect(cx - 50, cy - 3, 100, 3, colors.eye);

    // Bow loops
    gfx.drawQuadBezier(cx, cy - 30, cx - 20, cy - 46, cx - 10, cy - 30,
                       colors.eye, 3);
    gfx.drawQuadBezier(cx, cy - 30, cx + 20, cy - 46, cx + 10, cy - 30,
                       colors.eye, 3);

    gfx.popTransform();

    gfx.drawText("FOR YOU", SCX, STATUS_H + 22, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 166);
}

static void render_gift_open(GfxEngine& gfx, float t, const ColorContext& colors) {
    float openAmt = clamp(t / 0.4f, 0.0f, 1.0f);
    int16_t cx = SCX, cy = CY + 16;

    // Box body
    gfx.fillRoundedRect(cx - 50, cy - 14, 100, 50, 4, colors.eye);

    // Lid lifting
    int16_t lidY = (int16_t)(-openAmt * 30.0f);
    gfx.pushTransform();
    gfx.translate(0, (float)lidY);
    gfx.rotate(-openAmt * 14.0f, (float)(cx - 50), (float)(cy - 14));
    gfx.fillRoundedRect(cx - 56, cy - 26, 112, 16, 3, colors.eye);
    gfx.popTransform();

    // Heart + sparkles bursting out
    if (openAmt > 0.4f) {
        float burstT = t - 0.4f;
        uint8_t op = (uint8_t)(burstT / 0.6f * 255);
        fillHeart(gfx, cx, (int16_t)(cy - 30 - burstT * 40), 18.0f + burstT * 8.0f,
                  colors.eye, op);
        for (int i = 0; i < 4; i++) {
            float a = (float)i / 4.0f * TAU;
            float r = 30.0f + burstT * 30.0f;
            fillStar(gfx,
                     (int16_t)(cx + cosf(a) * r),
                     (int16_t)(cy - 20 + sinf(a) * r * 0.6f),
                     6.0f, 2.0f, colors.eye, (uint8_t)((1.0f - burstT) * 255));
        }
    }
}

const VariantDef GIFT_VARIANTS[] = {
    {"gift-wrapped", "Wrapped",  1600, TONE_ROSE, render_gift_wrapped},
    {"gift-open",    "Unboxing", 2400, TONE_WARM, render_gift_open},
};

extern const CategoryDef CAT_GIFT = {
    "gift", "Gift", ContentKind::SCENE, TONE_ROSE,
    GIFT_VARIANTS, sizeof(GIFT_VARIANTS) / sizeof(GIFT_VARIANTS[0])
};
