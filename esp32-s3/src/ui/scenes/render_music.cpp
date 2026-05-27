#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_music_notes(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = 60, cy = CY + 4;

    gfx.pushTransform();
    gfx.rotate(t * 360.0f, (float)cx, (float)cy);
    gfx.fillCircle(cx, cy, 42, colors.eye);
    gfx.fillCircle(cx, cy, 28, colors.bg);
    gfx.fillCircle(cx, cy, 20, colors.eye);
    gfx.fillCircle(cx, cy, 6, colors.bg);
    gfx.drawLine(cx, (int16_t)(cy - 16), cx, (int16_t)(cy - 12), colors.bg, 2);
    gfx.popTransform();

    for (int i = 0; i < 4; i++) {
        float p = fmodf(t * 0.8f + i * 0.25f, 1.0f);
        float x = lerp(120.0f, (float)(SCREEN_W - 24), p);
        float y = (float)CY + sinf(p * TAU * 1.5f + (float)i) * 22.0f;
        float fadeIn = fminf(1.0f, p * 4.0f);
        float fadeOut = fminf(1.0f, (1.0f - p) * 4.0f);
        uint8_t op = (uint8_t)(fadeIn * fadeOut * 255.0f);

        gfx.fillEllipse((int16_t)x, (int16_t)(y + 8), 6, 5, colors.eye, op);
        gfx.drawLine((int16_t)(x + 5), (int16_t)(y + 6),
                     (int16_t)(x + 5), (int16_t)(y - 18), colors.eye, 3, op);
        gfx.pushAlpha(op);
        gfx.drawQuadBezier((int16_t)(x + 5), (int16_t)(y - 18),
                           (int16_t)(x + 16), (int16_t)(y - 14),
                           (int16_t)(x + 14), (int16_t)(y - 4),
                           colors.eye, 3);
        gfx.popAlpha();
    }
}

static void render_music_bars(GfxEngine& gfx, float t, const ColorContext& colors) {
    int n = 14;
    int gap = 4;
    int16_t bw = (SCREEN_W - 60 - gap * (n - 1)) / n;

    for (int i = 0; i < n; i++) {
        float phase = t * TAU * 2.0f + i * 0.5f;
        int16_t h = (int16_t)(14.0f + fabsf(sinf(phase)) * 80.0f);
        int16_t x = (int16_t)(30 + i * (bw + gap));
        int16_t y = SCREEN_H - 28 - h;
        gfx.fillRoundedRect(x, y, bw, h, 3, colors.eye);
    }

    gfx.drawLine(20, SCREEN_H - 24, SCREEN_W - 20, SCREEN_H - 24,
                 colors.eye, 2, 102);
}

static void render_music_wave(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t baseY = CY + 8;
    int samples = 80;

    for (int i = 0; i < samples; i++) {
        float f0 = (float)i / (float)samples;
        float f1 = (float)(i + 1) / (float)samples;
        int16_t x0 = (int16_t)(20.0f + f0 * (SCREEN_W - 40));
        int16_t x1 = (int16_t)(20.0f + f1 * (SCREEN_W - 40));
        float phase0 = f0 * TAU * 3.0f + t * TAU * 2.0f;
        float phase1 = f1 * TAU * 3.0f + t * TAU * 2.0f;
        int16_t y0 = (int16_t)(baseY + sinf(phase0) * 30.0f * fabsf(sinf(f0 * PI)));
        int16_t y1 = (int16_t)(baseY + sinf(phase1) * 30.0f * fabsf(sinf(f1 * PI)));
        gfx.drawLine(x0, y0, x1, y1, colors.eye, 3);
    }

    int16_t sy = STATUS_H + 8;
    gfx.fillTriangle(SCX - 8, sy + 12, SCX + 8, sy + 22, SCX - 8, sy + 32,
                     colors.eye);
}

const VariantDef MUSIC_VARIANTS[] = {
    {"music-notes", "Notes",     3000, TONE_ROSE,   render_music_notes},
    {"music-bars",  "Equalizer", 1200, TONE_CYAN,   render_music_bars},
    {"music-wave",  "Waveform",  1600, TONE_PURPLE, render_music_wave},
};

extern const CategoryDef CAT_MUSIC = {
    "music", "Music", ContentKind::SCENE, TONE_ROSE,
    MUSIC_VARIANTS, sizeof(MUSIC_VARIANTS) / sizeof(MUSIC_VARIANTS[0])
};
