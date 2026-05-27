#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_navigation_compass(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX, cy = CY + 4;
    int16_t r = 64;

    float ang = sinf(t * TAU * 0.6f) * 28.0f + sinf(t * TAU * 3.0f) * 3.0f;

    gfx.strokeCircle(cx, cy, r, colors.eye, 3);

    for (int i = 0; i < 24; i++) {
        float a = (float)i / 24.0f * TAU;
        bool isLong = (i % 6 == 0);
        int16_t inner = isLong ? r - 14 : r - 6;
        gfx.drawLine((int16_t)(cx + cosf(a) * inner), (int16_t)(cy + sinf(a) * inner),
                     (int16_t)(cx + cosf(a) * (r - 1)), (int16_t)(cy + sinf(a) * (r - 1)),
                     colors.eye, isLong ? 3 : 1);
    }

    gfx.drawText("N", cx, cy - r + 24, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 217);
    gfx.drawText("S", cx, cy + r - 12, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 140);
    gfx.drawText("W", cx - r + 10, cy + 4, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 140);
    gfx.drawText("E", cx + r - 10, cy + 4, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 140);

    // Needle
    gfx.pushTransform();
    gfx.rotate(ang, (float)cx, (float)cy);
    gfx.fillTriangle(cx, cy - r + 6, cx - 6, cy + 8, cx + 6, cy + 8, colors.eye);
    gfx.fillTriangle(cx, cy + r - 6, cx - 6, cy - 8, cx + 6, cy - 8, colors.eye, 89);
    gfx.popTransform();

    gfx.fillCircle(cx, cy, 3, colors.eye);
}

static void render_navigation_pin(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX, cy = CY;

    // Grid lines
    for (int i = 0; i < 8; i++) {
        gfx.drawLine(0, (int16_t)(STATUS_H + 8 + i * 24), SCREEN_W,
                     (int16_t)(STATUS_H + 8 + i * 24), colors.eye, 1, 46);
    }
    for (int i = 0; i < 11; i++) {
        gfx.drawLine((int16_t)(i * 30), STATUS_H, (int16_t)(i * 30), SCREEN_H,
                     colors.eye, 1, 46);
    }

    // Pulsing rings
    for (int w = 0; w < 2; w++) {
        float p = fmodf(t * 1.5f + w * 0.5f, 1.0f);
        int16_t rr = (int16_t)lerp(8.0f, 60.0f, p);
        uint8_t op = (uint8_t)((1.0f - p) * 0.6f * 255);
        gfx.drawArc(cx, cy, rr, 0, TAU, colors.eye, 2, op);
    }

    // Pin shape: teardrop
    gfx.fillCircle(cx, cy - 16, 14, colors.eye);
    gfx.fillTriangle(cx - 12, cy - 12, cx + 12, cy - 12, cx, cy + 12, colors.eye);
    gfx.fillCircle(cx, cy - 16, 6, colors.bg);
}

static void render_navigation_arrow(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t x0 = 24, y0 = SCREEN_H - 30;
    int16_t x1 = SCREEN_W - 30, y1 = STATUS_H + 16;

    // Curved route path (quadratic bezier)
    int16_t midX = (x0 + x1) / 2;
    int16_t midY = (y0 + y1) / 2 - 60;
    gfx.pushAlpha(178);
    gfx.drawQuadBezier(x0, y0, midX, midY, x1, y1, colors.eye, 3);
    gfx.popAlpha();

    // Start pin
    gfx.fillCircle(x0, y0, 6, colors.eye);

    // End pin (teardrop)
    gfx.fillCircle(x1, y1 - 8, 8, colors.eye);
    gfx.fillTriangle(x1 - 6, y1 - 6, x1 + 6, y1 - 6, x1, y1 + 6, colors.eye);

    // Moving arrow along bezier curve
    float u = t;
    float u1 = 1.0f - u;
    float bx = u1 * u1 * x0 + 2 * u1 * u * midX + u * u * x1;
    float by = u1 * u1 * y0 + 2 * u1 * u * midY + u * u * y1;
    float dp = 0.02f;
    float v = fminf(t + dp, 1.0f);
    float v1 = 1.0f - v;
    float bx2 = v1 * v1 * x0 + 2 * v1 * v * midX + v * v * x1;
    float by2 = v1 * v1 * y0 + 2 * v1 * v * midY + v * v * y1;
    float ang = atan2f(by2 - by, bx2 - bx) * 180.0f / PI;

    gfx.pushTransform();
    gfx.translate(bx, by);
    gfx.rotate(ang, 0, 0);
    gfx.fillTriangle(-10, -8, 14, 0, -10, 8, colors.eye);
    gfx.popTransform();

    gfx.drawText("320 M  3 MIN", SCX, SCREEN_H - 8, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 166);
}

const VariantDef NAVIGATION_VARIANTS[] = {
    {"navigation-compass", "Compass",     4800, TONE_CYAN,  render_navigation_compass},
    {"navigation-pin",     "GPS pin",     1800, TONE_RED,   render_navigation_pin},
    {"navigation-arrow",   "Route arrow", 3000, TONE_GREEN, render_navigation_arrow},
};

extern const CategoryDef CAT_NAVIGATION = {
    "navigation", "Navigation", ContentKind::SCENE, TONE_CYAN,
    NAVIGATION_VARIANTS, sizeof(NAVIGATION_VARIANTS) / sizeof(NAVIGATION_VARIANTS[0])
};
