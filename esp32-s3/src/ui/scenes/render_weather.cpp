#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void drawCloud(GfxEngine& gfx, int16_t cx, int16_t cy,
                      uint16_t color, uint8_t alpha = 255) {
    gfx.fillEllipse(cx - 14, cy, 20, 16, color, alpha);
    gfx.fillEllipse(cx + 14, cy, 22, 18, color, alpha);
    gfx.fillEllipse(cx, cy - 10, 18, 14, color, alpha);
    gfx.fillRoundedRect(cx - 30, cy + 4, 62, 14, 7, color, alpha);
}

static void render_weather_sunny(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX - 60, cy = CY - 10;

    gfx.pushTransform();
    gfx.translate((float)cx, (float)cy);
    gfx.rotate(t * 180.0f, 0, 0);
    for (int i = 0; i < 8; i++) {
        float a = (float)i / 8.0f * TAU;
        gfx.drawLine((int16_t)(cosf(a) * 36), (int16_t)(sinf(a) * 36),
                     (int16_t)(cosf(a) * 52), (int16_t)(sinf(a) * 52),
                     colors.eye, 4);
    }
    gfx.popTransform();

    gfx.fillCircle(cx, cy, 28, colors.eye);
    gfx.fillCircle(cx, cy, 20, colors.bg);

    gfx.drawText("28\xB0", SCX + 52, cy, colors.eye, 4, GfxEngine::TextAlign::CENTER);
    gfx.drawText("SUNNY", SCX + 52, cy + 30, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 178);
}

static void render_weather_rainy(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX - 60, cy = CY - 14;
    drawCloud(gfx, cx, cy, colors.eye);

    for (int i = 0; i < 7; i++) {
        float p = fmodf(t * 2.0f + i * 0.14f, 1.0f);
        int16_t x = cx - 24 + i * 8;
        int16_t y = (int16_t)(cy + 22.0f + p * 50.0f);
        gfx.drawLine(x, y, (int16_t)(x - 4), (int16_t)(y + 10),
                     colors.eye, 3, (uint8_t)((1.0f - p) * 255));
    }

    gfx.drawText("18\xB0", SCX + 52, cy + 4, colors.eye, 4, GfxEngine::TextAlign::CENTER);
    gfx.drawText("RAINY", SCX + 52, cy + 34, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 178);
}

static void render_weather_cloudy(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX - 60, cy = CY - 8;
    float drift = sinf(t * TAU) * 4.0f;

    gfx.pushTransform();
    gfx.translate(drift, 0);
    gfx.fillEllipse(cx - 16, cy + 2, 16, 12, colors.eye);
    gfx.fillEllipse(cx + 16, cy + 2, 18, 14, colors.eye);
    gfx.fillEllipse(cx, cy - 6, 14, 10, colors.eye);
    gfx.fillRoundedRect(cx - 28, cy + 6, 56, 10, 5, colors.eye);
    gfx.popTransform();

    gfx.pushTransform();
    gfx.translate(-drift, -22.0f);
    gfx.fillEllipse(cx - 24, cy + 2, 12, 9, colors.eye, 128);
    gfx.fillEllipse(cx, cy + 2, 14, 11, colors.eye, 128);
    gfx.fillEllipse(cx - 12, cy - 4, 10, 8, colors.eye, 128);
    gfx.popTransform();

    gfx.drawText("22", SCX + 52, cy + 4, colors.eye, 4, GfxEngine::TextAlign::CENTER);
    gfx.drawText("CLOUDY", SCX + 52, cy + 34, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 178);
}

static void render_weather_snow(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX - 60, cy = CY - 14;

    gfx.fillEllipse(cx - 14, cy, 20, 16, colors.eye);
    gfx.fillEllipse(cx + 14, cy, 22, 18, colors.eye);
    gfx.fillRoundedRect(cx - 30, cy + 4, 62, 14, 7, colors.eye);

    for (int i = 0; i < 9; i++) {
        float p = fmodf(t * 0.8f + i * 0.11f, 1.0f);
        float x = (float)(cx - 30 + i * 8) + sinf(p * TAU * 2.0f + (float)i) * 4.0f;
        float y = (float)(cy + 22) + p * 56.0f;
        gfx.fillCircle((int16_t)x, (int16_t)y, 3, colors.eye,
                       (uint8_t)((1.0f - p * 0.4f) * 255));
    }

    gfx.drawText("-2\xB0", SCX + 52, cy + 4, colors.eye, 4, GfxEngine::TextAlign::CENTER);
    gfx.drawText("SNOW", SCX + 52, cy + 34, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 178);
}

static void render_weather_storm(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t cx = SCX - 60, cy = CY - 14;

    gfx.fillEllipse(cx - 14, cy, 20, 16, colors.eye);
    gfx.fillEllipse(cx + 14, cy, 22, 18, colors.eye);
    gfx.fillRoundedRect(cx - 30, cy + 4, 62, 14, 7, colors.eye);

    bool flash = (t > 0.4f && t < 0.5f) || (t > 0.7f && t < 0.74f);
    uint8_t boltOp = flash ? 255 : 128;
    gfx.fillTriangle(cx - 6, cy + 22, cx + 4, cy + 38, cx - 2, cy + 38,
                     colors.eye, boltOp);
    gfx.fillTriangle(cx - 2, cy + 38, cx + 8, cy + 60, cx + 2, cy + 44,
                     colors.eye, boltOp);
    gfx.fillTriangle(cx - 4, cy + 44, cx + 2, cy + 44, cx - 2, cy + 38,
                     colors.eye, boltOp);

    for (int i = 0; i < 5; i++) {
        float p = fmodf(t * 2.5f + i * 0.2f, 1.0f);
        int16_t x = cx - 24 + i * 12;
        int16_t y = (int16_t)(cy + 24.0f + p * 50.0f);
        gfx.drawLine(x, y, (int16_t)(x - 4), (int16_t)(y + 10),
                     colors.eye, 3, (uint8_t)((1.0f - p) * 255));
    }

    gfx.drawText("15\xB0", SCX + 52, cy + 4, colors.eye, 4, GfxEngine::TextAlign::CENTER);
    gfx.drawText("STORM", SCX + 52, cy + 34, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 178);
}

const VariantDef WEATHER_VARIANTS[] = {
    {"weather-sunny",  "Sunny",  4000, TONE_WARM,   render_weather_sunny},
    {"weather-rainy",  "Rainy",  1800, TONE_BLUE,   render_weather_rainy},
    {"weather-cloudy", "Cloudy", 5000, TONE_CYAN,   render_weather_cloudy},
    {"weather-snow",   "Snow",   4500, TONE_WHITE,  render_weather_snow},
    {"weather-storm",  "Storm",  2400, TONE_PURPLE, render_weather_storm},
};

extern const CategoryDef CAT_WEATHER = {
    "weather", "Weather", ContentKind::SCENE, TONE_CYAN,
    WEATHER_VARIANTS, sizeof(WEATHER_VARIANTS) / sizeof(WEATHER_VARIANTS[0])
};
