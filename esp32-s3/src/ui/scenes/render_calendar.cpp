#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_calendar_date(GfxEngine& gfx, float t, const ColorContext& colors) {
    float wob = sinf(t * TAU) * 1.2f;
    int16_t sy = STATUS_H + 8;

    gfx.pushTransform();
    gfx.rotate(wob, (float)SCX, (float)(CY + 4));

    // Card outline
    int16_t cardTop = sy + 12;
    int16_t cardH = SCREEN_H - sy - 24;
    gfx.strokeRoundedRect(SCX - 80, cardTop, 160, cardH, 8, colors.eye, 3);

    // Header bar (filled)
    gfx.fillRoundedRect(SCX - 80, cardTop, 160, 28, 8, colors.eye);
    gfx.drawText("MAY", SCX, cardTop + 18, colors.bg, 1,
                 GfxEngine::TextAlign::CENTER);

    // Big day number
    gfx.drawText("27", SCX, sy + 86, colors.eye, 7, GfxEngine::TextAlign::CENTER);

    // Weekday
    gfx.drawText("TUE", SCX, SCREEN_H - 36, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 204);

    // Calendar rings
    gfx.fillCircle(SCX - 36, cardTop - 4, 3, colors.bg);
    gfx.strokeCircle(SCX - 36, cardTop - 4, 3, colors.eye, 2);
    gfx.fillCircle(SCX + 36, cardTop - 4, 3, colors.bg);
    gfx.strokeCircle(SCX + 36, cardTop - 4, 3, colors.eye, 2);

    gfx.popTransform();
}

static void render_calendar_reminder(GfxEngine& gfx, float t, const ColorContext& colors) {
    float shake = sinf(t * TAU * 10.0f) * 4.0f;

    // Bell
    gfx.pushTransform();
    gfx.translate((float)(SCX - 50), (float)(CY + 6));
    gfx.rotate(shake, 0, 0);

    gfx.fillTriangle(0, -32, -22, 6, 22, 6, colors.eye);
    gfx.fillRoundedRect(-28, 0, 56, 10, 4, colors.eye);
    gfx.fillCircle(0, 14, 5, colors.eye);
    gfx.fillRoundedRect(-2, -40, 4, 8, 2, colors.eye);

    gfx.popTransform();

    // Notification badge
    gfx.fillCircle(SCX - 26, CY - 18, 11, colors.bg);
    gfx.strokeCircle(SCX - 26, CY - 18, 11, colors.eye, 3);
    gfx.drawText("3", SCX - 26, CY - 14, colors.eye, 1, GfxEngine::TextAlign::CENTER);

    // Right-side text
    gfx.drawText("MEETING", SCX + 32, CY - 10, colors.eye, 2,
                 GfxEngine::TextAlign::LEFT, 230);
    gfx.drawText("14:00", SCX + 32, CY + 10, colors.eye, 1,
                 GfxEngine::TextAlign::LEFT, 166);
    gfx.drawText("IN 10 MIN", SCX + 32, CY + 28, colors.eye, 1,
                 GfxEngine::TextAlign::LEFT, 140);
}

const VariantDef CALENDAR_VARIANTS[] = {
    {"calendar-date",     "Date page", 5000, TONE_CYAN, render_calendar_date},
    {"calendar-reminder", "Reminder",  1400, TONE_RED,  render_calendar_reminder},
};

extern const CategoryDef CAT_CALENDAR = {
    "calendar", "Calendar", ContentKind::SCENE, TONE_CYAN,
    CALENDAR_VARIANTS, sizeof(CALENDAR_VARIANTS) / sizeof(CALENDAR_VARIANTS[0])
};
