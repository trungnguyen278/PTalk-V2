#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_plant_grow(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t groundY = SCREEN_H - 28;
    float grow = ease::out(clamp(t, 0.0f, 1.0f));
    int16_t stemTop = (int16_t)(groundY - 40.0f - grow * 60.0f);
    bool flower = t > 0.7f;

    // Pot
    gfx.fillTriangle(SCX - 28, groundY, SCX + 28, groundY,
                     SCX + 22, groundY + 24, colors.eye);
    gfx.fillTriangle(SCX - 28, groundY, SCX + 22, groundY + 24,
                     SCX - 22, groundY + 24, colors.eye);
    gfx.fillRoundedRect(SCX - 32, groundY - 6, 64, 8, 2, colors.eye);

    // Stem
    gfx.drawLine(SCX, groundY, SCX, stemTop, colors.eye, 4);

    // Leaves
    if (grow > 0.3f) {
        uint8_t leafOp = (uint8_t)((grow - 0.3f) / 0.7f * 255);
        gfx.pushTransform();
        gfx.rotate(-30.0f, (float)(SCX - 18), (float)(stemTop + 26));
        gfx.fillEllipse(SCX - 18, stemTop + 26, 14, 7, colors.eye, leafOp);
        gfx.popTransform();

        gfx.pushTransform();
        gfx.rotate(30.0f, (float)(SCX + 18), (float)(stemTop + 14));
        gfx.fillEllipse(SCX + 18, stemTop + 14, 14, 7, colors.eye, leafOp);
        gfx.popTransform();
    }

    // Flower head
    if (flower) {
        uint8_t flowerOp = (uint8_t)((t - 0.7f) / 0.3f * 255);
        for (int i = 0; i < 6; i++) {
            float a = (float)i / 6.0f * TAU;
            int16_t px = (int16_t)(SCX + cosf(a) * 14);
            int16_t py = (int16_t)(stemTop + sinf(a) * 14);
            gfx.pushTransform();
            gfx.rotate((float)i / 6.0f * 360.0f, (float)px, (float)py);
            gfx.fillEllipse(px, py, 10, 6, colors.eye, flowerOp);
            gfx.popTransform();
        }
        gfx.fillCircle(SCX, stemTop, 8, colors.bg, flowerOp);
    }
}

static void render_plant_water(GfxEngine& gfx, float t, const ColorContext& colors) {
    float tilt = clamp((t - 0.1f) / 0.4f, 0.0f, 1.0f) * 26.0f;
    bool drops = t > 0.4f;
    int16_t cx = SCX - 30, cy = CY - 18;

    // Watering can
    gfx.pushTransform();
    gfx.rotate(tilt, (float)cx, (float)cy);
    gfx.fillRoundedRect(cx - 24, cy - 14, 48, 28, 4, colors.eye);
    gfx.fillRect(cx - 36, cy - 4, 12, 6, colors.eye);
    gfx.fillRoundedRect(cx - 44, cy - 14, 8, 20, 3, colors.eye);
    // Spout
    gfx.drawLine(cx + 24, cy - 14, cx + 36, cy + 2, colors.eye, 4);
    gfx.popTransform();

    // Water drops
    if (drops) {
        for (int i = 0; i < 5; i++) {
            float p = fmodf((t - 0.4f) * 3.0f + i * 0.18f, 1.0f);
            int16_t dx = (int16_t)(cx + 38 + i * 4);
            int16_t dy = (int16_t)(cy + 12.0f + p * 60.0f);
            gfx.fillEllipse(dx, dy, 3, 5, colors.eye, (uint8_t)((1.0f - p) * 255));
        }
    }

    // Plant on right
    int16_t px = SCX + 56, py = SCREEN_H - 30;
    gfx.fillTriangle(px - 16, py, px + 16, py, px + 12, py + 18, colors.eye);
    gfx.fillTriangle(px - 16, py, px + 12, py + 18, px - 12, py + 18, colors.eye);
    gfx.drawLine(px, py, px, py - 32, colors.eye, 3);

    gfx.pushTransform();
    gfx.rotate(-30.0f, (float)(px - 12), (float)(py - 20));
    gfx.fillEllipse(px - 12, py - 20, 10, 6, colors.eye);
    gfx.popTransform();

    gfx.pushTransform();
    gfx.rotate(30.0f, (float)(px + 12), (float)(py - 26));
    gfx.fillEllipse(px + 12, py - 26, 10, 6, colors.eye);
    gfx.popTransform();
}

const VariantDef PLANT_VARIANTS[] = {
    {"plant-grow",  "Growing",  4000, TONE_GREEN, render_plant_grow},
    {"plant-water", "Watering", 2200, TONE_BLUE,  render_plant_water},
};

extern const CategoryDef CAT_PLANT = {
    "plant", "Plant", ContentKind::SCENE, TONE_GREEN,
    PLANT_VARIANTS, sizeof(PLANT_VARIANTS) / sizeof(PLANT_VARIANTS[0])
};
