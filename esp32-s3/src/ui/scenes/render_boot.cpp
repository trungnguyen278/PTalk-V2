#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>
#include <cstdio>

using namespace geom;
using namespace math;

// boot-poweron: dot → slit → split → two eyes
static void render_boot_poweron(GfxEngine& gfx, float t, const ColorContext& colors) {
    if (t < 0.2f) {
        // Tiny dot grows to a small rect
        float p = ease::out(t / 0.2f);
        int16_t r = (int16_t)lerp(1.0f, 8.0f, p);
        gfx.fillRoundedRect(SCX - r, CY - r, r * 2, r * 2, r, colors.eye);
    } else if (t < 0.5f) {
        // Horizontal bar stretches
        float p = ease::inOut((t - 0.2f) / 0.3f);
        int16_t bw = (int16_t)lerp(16.0f, (float)(GAP + EYE_W), p);
        int16_t bh = (int16_t)lerp(16.0f, 14.0f, p);
        int16_t rx = (int16_t)fminf((float)EYE_RX, bh / 2.0f);
        gfx.fillRoundedRect(SCX - bw / 2, CY - bh / 2, bw, bh, rx, colors.eye);
    } else if (t < 0.8f) {
        // Bar splits into two eye blocks
        float p = ease::inOut((t - 0.5f) / 0.3f);
        int16_t bh = (int16_t)lerp(14.0f, (float)EYE_H * 0.6f, p);
        int16_t rx = (int16_t)fminf((float)EYE_RX, bh / 2.0f);

        int16_t lcx = (int16_t)lerp((float)SCX, (float)LX, p);
        int16_t rcx = (int16_t)lerp((float)SCX, (float)RX, p);
        int16_t hw = (int16_t)lerp((float)(GAP + EYE_W) / 2.0f, (float)EYE_W / 2.0f, p);

        gfx.fillRoundedRect(lcx - hw, CY - bh / 2, hw * 2, bh, rx, colors.eye);
        gfx.fillRoundedRect(rcx - hw, CY - bh / 2, hw * 2, bh, rx, colors.eye);
    } else {
        // Settle to default eye proportions
        float p = ease::inOut((t - 0.8f) / 0.2f);
        int16_t bh = (int16_t)lerp((float)EYE_H * 0.6f, (float)EYE_H, p);
        gfx.drawEye(LX, CY, EYE_W, bh, EYE_RX, 0, colors.eye);
        gfx.drawEye(RX, CY, EYE_W, bh, EYE_RX, 0, colors.eye);
    }
}

// boot-logo: "PTIT" text with sweeping ring
static void render_boot_logo(GfxEngine& gfx, float t, const ColorContext& colors) {
    float enter = clamp(t / 0.4f, 0.0f, 1.0f);
    float scale = lerp(0.5f, 1.0f, ease::out(enter));
    uint8_t op = (uint8_t)(ease::out(enter) * 255.0f);

    // Sweeping ring
    float sweepAngle = t * TAU;
    gfx.strokeCircle(SCX, SCY - 6, 64, colors.accent, 1);
    gfx.drawArc(SCX, SCY - 6, 64, sweepAngle - 1.4f, sweepAngle,
                colors.accent, 3, op);

    // PTIT text centered (scale 2 = 16px height)
    uint8_t sc = (uint8_t)(2.0f * scale);
    if (sc < 1) sc = 1;
    gfx.drawText("PTIT", SCX, SCY - 10, colors.eye, sc,
                 GfxEngine::TextAlign::CENTER, op);

    // Subtitle
    if (t > 0.5f) {
        float subOp = clamp((t - 0.5f) * 3.0f, 0.0f, 0.7f);
        gfx.drawText("PTalk v2.0", SCX, SCREEN_H - 12, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, (uint8_t)(subOp * 255.0f));
    }
}

// boot-checks: system self-test checklist
static void render_boot_checks(GfxEngine& gfx, float t, const ColorContext& colors) {
    // Title
    gfx.drawText("SYSTEM CHECK", SCX, STATUS_H + 18, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 217);

    // Divider
    gfx.drawLine(48, STATUS_H + 28, SCREEN_W - 48, STATUS_H + 28,
                 colors.eye, 1, 89);

    static const char* ITEMS[] = {"DISPLAY", "AUDIO", "MIC", "NETWORK", "AI CORE"};
    for (int i = 0; i < 5; i++) {
        float start = i * 0.15f;
        float showT = clamp((t - start) / 0.1f, 0.0f, 1.0f);
        if (showT <= 0.0f) continue;

        float okT = clamp((t - start - 0.1f) / 0.06f, 0.0f, 1.0f);
        int16_t y = STATUS_H + 46 + i * 24;
        uint8_t rowOp = (uint8_t)(ease::out(showT) * 255.0f);

        // Item label
        gfx.drawText(ITEMS[i], 48, y, colors.eye, 1,
                     GfxEngine::TextAlign::LEFT, rowOp);

        // Status: spinner or OK
        if (okT < 1.0f) {
            // Spinning arc
            float ang = t * TAU * 4.0f;
            gfx.drawArc(SCREEN_W - 60, y + 2, 8, ang, ang + 2.0f,
                        colors.accent, 2, rowOp);
        } else {
            gfx.drawText("OK", SCREEN_W - 60, y, colors.accent, 1,
                         GfxEngine::TextAlign::LEFT, rowOp);
        }
    }

    // Footer
    gfx.drawText("PTIT  PTalk v2.0", SCX, SCREEN_H - 12, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 140);
}

// boot-ready: progress bar fills, READY stamps in
static void render_boot_ready(GfxEngine& gfx, float t, const ColorContext& colors) {
    float fill = clamp(t / 0.7f, 0.0f, 1.0f);
    float stampT = clamp((t - 0.75f) / 0.15f, 0.0f, 1.0f);

    // Small PTIT text at top
    gfx.drawText("PTIT", SCX, STATUS_H + 34, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 242);

    // Progress bar
    int16_t barW = SCREEN_W - 96;
    int16_t barX = (SCREEN_W - barW) / 2;
    int16_t barY = SCY + 8;

    gfx.strokeRoundedRect(barX, barY, barW, 10, 3, colors.eye, 1);
    int16_t fw = (int16_t)((float)(barW - 4) * fill);
    if (fw > 0) {
        gfx.fillRoundedRect(barX + 2, barY + 2, fw, 6, 2, colors.accent);
    }

    // Percentage text
    char pctBuf[8];
    snprintf(pctBuf, sizeof(pctBuf), "%03d%%", (int)(fill * 100.0f));
    gfx.drawText(pctBuf, SCX, barY + 24, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 191);

    // READY stamp
    if (stampT > 0.02f) {
        uint8_t sop = (uint8_t)(stampT * 255.0f);
        gfx.strokeRoundedRect(SCX - 44, SCREEN_H - 36, 88, 22, 3,
                              colors.accent, 2);
        gfx.drawText("READY", SCX, SCREEN_H - 22, colors.accent, 2,
                     GfxEngine::TextAlign::CENTER, sop);
    }
}

// --- Category registration ---
const VariantDef BOOT_VARIANTS[] = {
    {"boot-poweron", "Power on",  2400, TONE_RED, render_boot_poweron},
    {"boot-logo",    "Logo",      3000, TONE_RED, render_boot_logo},
    {"boot-checks",  "Self-test", 4200, TONE_RED, render_boot_checks},
    {"boot-ready",   "Ready",     3000, TONE_RED, render_boot_ready},
};

extern const CategoryDef CAT_BOOT = {
    "boot", "Boot", ContentKind::SCENE, TONE_RED,
    BOOT_VARIANTS, sizeof(BOOT_VARIANTS) / sizeof(BOOT_VARIANTS[0])
};
