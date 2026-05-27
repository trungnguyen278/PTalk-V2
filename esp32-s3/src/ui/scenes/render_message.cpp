#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_message_typing(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX, cy = CY + 4;

    // Bubble
    gfx.fillRoundedRect(cx - 64, cy - 26, 128, 52, 26, colors.eye);
    // Tail
    gfx.fillTriangle(cx - 30, cy + 26, cx - 14, cy + 26, cx - 28, cy + 40,
                     colors.eye);

    // Typing dots (bg-colored circles inside bubble)
    for (int i = 0; i < 3; i++) {
        float phase = fmodf(t - i * 0.18f + 1.0f, 1.0f);
        float rr = phase < 0.4f ? 4.0f + (phase / 0.4f) * 4.0f
                                : 8.0f - (phase - 0.4f) * 4.0f;
        int16_t r = (int16_t)clamp(rr, 3.0f, 8.0f);
        gfx.fillCircle((int16_t)(cx - 24 + i * 24), cy, r, colors.bg);
    }
}

static void render_message_envelope(GfxEngine& gfx, float t, const ColorContext& colors) {
    float openAmt = pulse(t, 0.55f, 0.25f);
    int16_t cx = SCX, cy = CY + 8;

    // Envelope body
    gfx.fillRoundedRect(cx - 60, cy - 30, 120, 70, 6, colors.eye);

    // Letter rising out
    if (openAmt > 0.1f) {
        int16_t letterY = (int16_t)(-openAmt * 28.0f);
        gfx.fillRoundedRect(cx - 50, cy - 24 + letterY, 100, 50, 2, colors.bg);
        gfx.strokeRoundedRect(cx - 50, cy - 24 + letterY, 100, 50, 2, colors.eye, 2);
        gfx.drawLine(cx - 36, cy - 10 + letterY, cx + 36, cy - 10 + letterY,
                     colors.eye, 2);
        gfx.drawLine(cx - 36, cy + letterY, cx + 36, cy + letterY, colors.eye, 2);
        gfx.drawLine(cx - 36, cy + 10 + letterY, cx + 12, cy + 10 + letterY,
                     colors.eye, 2);
    }

    // Envelope flap (V-shape over the top)
    int16_t flapY = (int16_t)(cy + 4.0f - openAmt * 30.0f);
    gfx.drawLine(cx - 60, cy - 30, cx, flapY, colors.bg, 4);
    gfx.drawLine(cx, flapY, cx + 60, cy - 30, colors.bg, 4);

    // Notification badge
    gfx.fillCircle(cx + 50, cy - 34, 11, colors.bg);
    gfx.strokeCircle(cx + 50, cy - 34, 11, colors.eye, 2);
    gfx.drawText("1", cx + 50, cy - 30, colors.eye, 1, GfxEngine::TextAlign::CENTER);
}

static void render_message_bubbles(GfxEngine& gfx, float t, const ColorContext& colors) {
    struct Bubble { int16_t x, y, w; bool isRight; float at; };
    static const Bubble ITEMS[] = {
        {(int16_t)(SCX - 60), (int16_t)(STATUS_H + 30), 80, false, 0.0f},
        {(int16_t)(SCX + 50), (int16_t)(STATUS_H + 72), 100, true,  0.33f},
        {(int16_t)(SCX - 50), (int16_t)(STATUS_H + 118), 70, false, 0.66f},
    };

    for (int i = 0; i < 3; i++) {
        float phase = clamp((t - ITEMS[i].at) / 0.25f, 0.0f, 1.0f);
        if (phase <= 0) continue;
        uint8_t op = (uint8_t)(phase * 255);

        if (ITEMS[i].isRight) {
            gfx.fillRoundedRect(ITEMS[i].x - ITEMS[i].w / 2, ITEMS[i].y - 12,
                                ITEMS[i].w, 24, 12, colors.eye, op);
        } else {
            gfx.pushAlpha(op);
            gfx.strokeRoundedRect(ITEMS[i].x - ITEMS[i].w / 2, ITEMS[i].y - 12,
                                  ITEMS[i].w, 24, 12, colors.eye, 2);
            gfx.popAlpha();
        }
    }
}

const VariantDef MESSAGE_VARIANTS[] = {
    {"message-typing",   "Typing",       1500, TONE_CYAN,  render_message_typing},
    {"message-envelope", "New mail",     2200, TONE_WHITE, render_message_envelope},
    {"message-bubbles",  "Chat bubbles", 3000, TONE_WARM,  render_message_bubbles},
};

extern const CategoryDef CAT_MESSAGE = {
    "message", "Message", ContentKind::SCENE, TONE_CYAN,
    MESSAGE_VARIANTS, sizeof(MESSAGE_VARIANTS) / sizeof(MESSAGE_VARIANTS[0])
};
