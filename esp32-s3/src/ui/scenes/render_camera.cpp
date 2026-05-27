#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_camera_shutter(GfxEngine& gfx, float t, const ColorContext& colors) {
    bool flash = t < 0.06f;
    int16_t cx = SCX, cy = CY + 4;

    // Viewfinder brackets (corners)
    int16_t bsz = 70;
    int16_t bl = 14;
    // Top-left
    gfx.drawLine(cx - bsz, cy - 40, cx - bsz, cy - 40 + bl, colors.eye, 3);
    gfx.drawLine(cx - bsz, cy - 40, cx - bsz + bl, cy - 40, colors.eye, 3);
    // Top-right
    gfx.drawLine(cx + bsz, cy - 40, cx + bsz, cy - 40 + bl, colors.eye, 3);
    gfx.drawLine(cx + bsz, cy - 40, cx + bsz - bl, cy - 40, colors.eye, 3);
    // Bottom-left
    gfx.drawLine(cx - bsz, cy + 40, cx - bsz, cy + 40 - bl, colors.eye, 3);
    gfx.drawLine(cx - bsz, cy + 40, cx - bsz + bl, cy + 40, colors.eye, 3);
    // Bottom-right
    gfx.drawLine(cx + bsz, cy + 40, cx + bsz, cy + 40 - bl, colors.eye, 3);
    gfx.drawLine(cx + bsz, cy + 40, cx + bsz - bl, cy + 40, colors.eye, 3);

    // Camera body
    gfx.fillRoundedRect(cx - 24, cy - 12, 48, 32, 4, colors.eye);
    gfx.fillCircle(cx, cy + 4, 11, colors.bg);
    gfx.fillCircle(cx, cy + 4, 6, colors.eye);
    gfx.fillRoundedRect(cx + 12, cy - 18, 10, 6, 1, colors.eye);

    if (flash) {
        gfx.fillRect(0, 0, SCREEN_W, SCREEN_H, colors.eye, 217);
    }

    gfx.drawText(t < 0.3f ? "SNAP!" : "SMILE", SCX, SCREEN_H - 28,
                 colors.eye, 1, GfxEngine::TextAlign::CENTER, 178);
}

static void render_camera_focus(GfxEngine& gfx, float t, const ColorContext& colors) {
    float target = ease::out(t < 0.7f ? t / 0.7f : 1.0f);
    float sz = lerp(90.0f, 36.0f, target);
    int16_t cx = SCX, cy = CY + 6;
    bool ok = t > 0.7f;
    int16_t half = (int16_t)(sz / 2.0f);
    int16_t arm = 12;

    // Focus brackets
    gfx.drawLine(cx - half, cy - half + arm, cx - half, cy - half, colors.eye, 3);
    gfx.drawLine(cx - half, cy - half, cx - half + arm, cy - half, colors.eye, 3);

    gfx.drawLine(cx + half - arm, cy - half, cx + half, cy - half, colors.eye, 3);
    gfx.drawLine(cx + half, cy - half, cx + half, cy - half + arm, colors.eye, 3);

    gfx.drawLine(cx - half, cy + half - arm, cx - half, cy + half, colors.eye, 3);
    gfx.drawLine(cx - half, cy + half, cx - half + arm, cy + half, colors.eye, 3);

    gfx.drawLine(cx + half - arm, cy + half, cx + half, cy + half, colors.eye, 3);
    gfx.drawLine(cx + half, cy + half, cx + half, cy + half - arm, colors.eye, 3);

    gfx.fillCircle(cx, cy, 3, colors.eye);

    if (ok) {
        uint8_t op = (uint8_t)((t - 0.7f) / 0.3f * 255);
        gfx.fillCircle(cx + 36, cy - 22, 12, colors.eye, op);
        // Checkmark inside circle
        gfx.drawLine(cx + 30, cy - 22, cx + 35, cy - 17, colors.bg, 3, op);
        gfx.drawLine(cx + 35, cy - 17, cx + 42, cy - 27, colors.bg, 3, op);
    }
}

const VariantDef CAMERA_VARIANTS[] = {
    {"camera-shutter", "Shutter",    2000, TONE_WHITE, render_camera_shutter},
    {"camera-focus",   "Auto focus", 1800, TONE_CYAN,  render_camera_focus},
};

extern const CategoryDef CAT_CAMERA = {
    "camera", "Camera", ContentKind::SCENE, TONE_WHITE,
    CAMERA_VARIANTS, sizeof(CAMERA_VARIANTS) / sizeof(CAMERA_VARIANTS[0])
};
