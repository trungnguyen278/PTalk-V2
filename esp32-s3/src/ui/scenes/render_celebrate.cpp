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

static void render_celebrate_trophy(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX, cy = CY + 16;
    float shine = sinf(t * TAU) * 0.3f + 0.7f;
    uint8_t shineOp = (uint8_t)(shine * 255);

    // Trophy cup body
    gfx.fillTriangle(cx - 24, cy - 30, cx + 24, cy - 30, cx + 22, cy + 4, colors.eye, shineOp);
    gfx.fillTriangle(cx - 24, cy - 30, cx + 22, cy + 4, cx - 22, cy + 4, colors.eye, shineOp);
    gfx.fillRoundedRect(cx - 8, cy + 12, 16, 12, 2, colors.eye, shineOp);
    gfx.fillRoundedRect(cx - 22, cy + 22, 44, 6, 2, colors.eye, shineOp);

    // Handles
    gfx.pushAlpha(shineOp);
    gfx.drawQuadBezier(cx - 24, cy - 22, cx - 40, cy - 18, cx - 36, cy - 4,
                       colors.eye, 3);
    gfx.drawQuadBezier(cx + 24, cy - 22, cx + 40, cy - 18, cx + 36, cy - 4,
                       colors.eye, 3);
    gfx.popAlpha();

    // Orbiting sparkle stars
    for (int i = 0; i < 4; i++) {
        float p = fmodf(t * 1.2f + i * 0.25f, 1.0f);
        float a = (float)i * (TAU / 4.0f) + t * TAU * 0.4f;
        int16_t sx = (int16_t)(cx + cosf(a) * 60);
        int16_t sy = (int16_t)(cy + sinf(a) * 30);
        float s = lerp(8.0f, 2.0f, p);
        fillStar(gfx, sx, sy, s, s * 0.4f, colors.eye, (uint8_t)((1.0f - p) * 255));
    }
}

static void render_celebrate_confetti(GfxEngine& gfx, float t, const ColorContext& colors) {
    static const ToneId PIECE_TONES[] = {
        TONE_WARM, TONE_ROSE, TONE_CYAN, TONE_GREEN, TONE_PURPLE, TONE_ORANGE
    };

    gfx.drawText("YAY!", SCX, CY + 8, colors.eye, 3, GfxEngine::TextAlign::CENTER);

    int16_t sy = STATUS_H + 8;
    for (int i = 0; i < 18; i++) {
        float p = fmodf(t + i * 0.06f, 1.0f);
        int16_t x = (int16_t)((i * 53 + (int)(sinf(p * TAU + (float)i) * 8.0f)) % SCREEN_W);
        int16_t y = (int16_t)lerp((float)sy, (float)(SCREEN_H - 8), p);
        uint8_t op = (uint8_t)((1.0f - p) * 0.9f * 255.0f);
        uint16_t pc = colors.tones[PIECE_TONES[i % 6]];

        int shape = i % 3;
        if (shape == 0) {
            gfx.fillRect(x - 3, y - 2, 6, 4, pc, op);
        } else if (shape == 1) {
            gfx.fillCircle(x, y, 3, pc, op);
        } else {
            gfx.fillRect(x - 2, y - 4, 3, 8, pc, op);
        }
    }
}

static void render_celebrate_firework(GfxEngine& gfx, float t, const ColorContext& colors) {
    struct Burst { int16_t x, y; float phase; ToneId tone; };
    static const Burst BURSTS[] = {
        {(int16_t)(SCX - 60), (int16_t)(CY - 6),  0.0f, TONE_WARM},
        {(int16_t)(SCX + 50), (int16_t)(CY + 14), 0.4f, TONE_ROSE},
        {SCX,                 (int16_t)(CY - 20),  0.7f, TONE_GREEN},
    };

    for (int b = 0; b < 3; b++) {
        float p = fmodf(t + BURSTS[b].phase, 1.0f);
        float r = lerp(2.0f, 50.0f, p);
        uint8_t op = (uint8_t)((1.0f - p) * 255);
        uint16_t bc = colors.tones[BURSTS[b].tone];

        for (int j = 0; j < 10; j++) {
            float a = (float)j / 10.0f * TAU;
            gfx.drawLine(
                (int16_t)(BURSTS[b].x + cosf(a) * r * 0.5f),
                (int16_t)(BURSTS[b].y + sinf(a) * r * 0.5f),
                (int16_t)(BURSTS[b].x + cosf(a) * r),
                (int16_t)(BURSTS[b].y + sinf(a) * r),
                bc, 3, op);
        }
        int16_t dotR = (int16_t)(3 + (1.0f - p) * 3);
        gfx.fillCircle(BURSTS[b].x, BURSTS[b].y, dotR, bc, op);
    }
}

const VariantDef CELEBRATE_VARIANTS[] = {
    {"celebrate-trophy",   "Trophy",    2400, TONE_WARM,  render_celebrate_trophy},
    {"celebrate-confetti", "Confetti",  3000, TONE_WARM,  render_celebrate_confetti},
    {"celebrate-firework", "Fireworks", 2000, TONE_WARM,  render_celebrate_firework},
};

extern const CategoryDef CAT_CELEBRATE = {
    "celebrate", "Celebrate", ContentKind::SCENE, TONE_WARM,
    CELEBRATE_VARIANTS, sizeof(CELEBRATE_VARIANTS) / sizeof(CELEBRATE_VARIANTS[0])
};
