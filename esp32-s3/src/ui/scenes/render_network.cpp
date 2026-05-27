#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>
#include <cstdio>

using namespace geom;
using namespace math;

// WiFi icon: 3 concentric arcs (top-half) + dot at bottom
static void drawWifiIcon(GfxEngine& gfx, int16_t cx, int16_t cy,
                         int bars, uint16_t color, uint8_t alpha = 255) {
    int16_t radii[] = {28, 18, 8};
    for (int i = 0; i < 3; i++) {
        uint8_t op = (i < (3 - bars)) ? (uint8_t)(alpha * 0.2f) : alpha;
        // Top-half arc: from -PI to 0 (left to right over top)
        gfx.drawArc(cx, cy, radii[i], -3.14159f, 0.0f, color, 5, op);
    }
    gfx.fillCircle(cx, cy + 3, 3, color, alpha);
}

// BT icon simplified as lines
static void drawBTIcon(GfxEngine& gfx, int16_t cx, int16_t cy,
                       uint16_t color, uint8_t alpha = 255) {
    // Vertical center line
    gfx.drawLine(cx, cy - 16, cx, cy + 16, color, 4);
    // Upper right arrow
    gfx.drawLine(cx, cy - 16, cx + 10, cy - 8, color, 4);
    gfx.drawLine(cx + 10, cy - 8, cx - 10, cy + 8, color, 4);
    // Lower right arrow
    gfx.drawLine(cx, cy + 16, cx + 10, cy + 8, color, 4);
    gfx.drawLine(cx + 10, cy + 8, cx - 10, cy - 8, color, 4);
}

// Typing dots animation
static void drawTypingDots(GfxEngine& gfx, int16_t cx, int16_t cy, float t,
                           uint16_t color) {
    int step = ((int)(t * 6.0f)) % 3;
    for (int i = -1; i <= 1; i++) {
        uint8_t op = (i == step - 1) ? 255 : 77;
        gfx.fillCircle((int16_t)(cx + i * 12), cy, 4, color, op);
    }
}

// network-wifi-scan: cycling WiFi bars + "SEARCHING" text
static void render_wifi_scan(GfxEngine& gfx, float t, const ColorContext& colors) {
    float phase = fmodf(t * 3.0f, 1.0f);
    int bars = phase < 0.33f ? 1 : phase < 0.66f ? 2 : 3;
    drawWifiIcon(gfx, SCX, SCY - 14, bars, colors.accent);

    gfx.drawText("SEARCHING WI-FI", SCX, SCREEN_H - 40, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 242);
    drawTypingDots(gfx, SCX, SCREEN_H - 20, t, colors.accent);
}

// network-wifi-connect: full bars blinking + progress bar + SSID
static void render_wifi_connect(GfxEngine& gfx, float t, const ColorContext& colors) {
    uint8_t blink = ((int)(t * 4.0f) % 2 == 0) ? 255 : 102;
    drawWifiIcon(gfx, SCX, STATUS_H + 56, 3, colors.accent, blink);

    gfx.drawText("CONNECTING", SCX, STATUS_H + 104, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER);
    gfx.drawText("PTIT-NETWORK", SCX, STATUS_H + 120, colors.accent, 1,
                 GfxEngine::TextAlign::CENTER, 217);

    // Progress bar
    int16_t barW = SCREEN_W - 80;
    int16_t barX = (SCREEN_W - barW) / 2;
    int16_t barY = SCREEN_H - 38;
    gfx.strokeRoundedRect(barX, barY, barW, 8, 2, colors.eye, 1);
    int16_t fw = (int16_t)((float)(barW - 3) * clamp(t, 0.0f, 1.0f));
    if (fw > 0) {
        gfx.fillRoundedRect(barX + 1, barY + 1, fw, 6, 1, colors.accent);
    }
}

// network-wifi-retry: spinning retry ring + attempt counter
static void render_wifi_retry(GfxEngine& gfx, float t, const ColorContext& colors) {
    drawWifiIcon(gfx, SCX, STATUS_H + 60, 1, colors.accent, 217);

    // Spinning retry arc
    float spin = t * TAU;
    gfx.drawArc(SCX, STATUS_H + 60, 42, spin, spin + 4.7f,
                colors.accent, 3);

    gfx.drawText("RETRY CONNECTION", SCX, STATUS_H + 126, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER);

    int attempt = ((int)(t * 3.0f)) + 1;
    char buf[20];
    snprintf(buf, sizeof(buf), "ATTEMPT %d OF 3", attempt);
    gfx.drawText(buf, SCX, SCREEN_H - 18, colors.accent, 1,
                 GfxEngine::TextAlign::CENTER);
}

// network-offline: crossed-out WiFi + "NO INTERNET"
static void render_offline(GfxEngine& gfx, float t, const ColorContext& colors) {
    drawWifiIcon(gfx, SCX, STATUS_H + 60, 0, colors.eye, 115);

    // Diagonal slash
    gfx.drawLine(SCX - 36, STATUS_H + 30, SCX + 36, STATUS_H + 100,
                 colors.accent, 5);

    float breathe = 0.7f + fabsf(sinf(t * TAU)) * 0.3f;
    gfx.drawText("NO INTERNET", SCX, STATUS_H + 130, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, (uint8_t)(breathe * 255.0f));
    gfx.drawText("CHECK WI-FI ROUTER", SCX, SCREEN_H - 18, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 140);
}

// network-ble-pair: BT icon with expanding pulse rings
static void render_ble_pair(GfxEngine& gfx, float t, const ColorContext& colors) {
    // Pulse rings
    for (int w = 0; w < 2; w++) {
        float p = fmodf(t + w * 0.5f, 1.0f);
        int16_t r = (int16_t)lerp(18.0f, 56.0f, p);
        uint8_t op = (uint8_t)((1.0f - p) * 0.7f * 255.0f);
        gfx.pushAlpha(op);
        gfx.strokeCircle(SCX, STATUS_H + 60, r, colors.accent, 2);
        gfx.popAlpha();
    }

    drawBTIcon(gfx, SCX, STATUS_H + 60, colors.accent);

    gfx.drawText("PAIRING MODE", SCX, STATUS_H + 126, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER);
    gfx.drawText("OPEN PTALK APP", SCX, SCREEN_H - 26, colors.accent, 1,
                 GfxEngine::TextAlign::CENTER, 217);
    gfx.drawText("TO CONNECT", SCX, SCREEN_H - 12, colors.accent, 1,
                 GfxEngine::TextAlign::CENTER, 140);
}

// network-server-error: exclamation mark + "SERVER UNREACHABLE"
static void render_server_error(GfxEngine& gfx, float t, const ColorContext& colors) {
    float shake = sinf(t * TAU * 6.0f) * 1.5f;
    uint8_t blink = ((int)(t * 3.0f) % 2 == 0) ? 255 : 128;

    gfx.pushTransform();
    gfx.translate(shake, 0.0f);

    // Exclamation mark
    gfx.fillRoundedRect(SCX - 3, STATUS_H + 38, 6, 20, 2, colors.accent, blink);
    gfx.fillCircle(SCX, STATUS_H + 66, 3, colors.accent, blink);

    gfx.drawText("SERVER UNREACHABLE", SCX, STATUS_H + 106, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER);
    gfx.drawText("ERROR 503", SCX, STATUS_H + 124, colors.accent, 1,
                 GfxEngine::TextAlign::CENTER, 217);
    gfx.drawText("RETRYING IN A MOMENT", SCX, SCREEN_H - 18, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 140);

    gfx.popTransform();
}

// --- Category registration ---
const VariantDef NETWORK_VARIANTS[] = {
    {"network-wifi-scan",     "Searching",   1800, TONE_CYAN,   render_wifi_scan},
    {"network-wifi-connect",  "Connecting",  2400, TONE_CYAN,   render_wifi_connect},
    {"network-wifi-retry",    "Retrying",    2000, TONE_ORANGE, render_wifi_retry},
    {"network-offline",       "Offline",     2400, TONE_RED,    render_offline},
    {"network-ble-pair",      "BLE pairing", 2200, TONE_PURPLE, render_ble_pair},
    {"network-server-error",  "Server down", 2400, TONE_RED,    render_server_error},
};

extern const CategoryDef CAT_NETWORK = {
    "network", "Network", ContentKind::SCENE, TONE_CYAN,
    NETWORK_VARIANTS, sizeof(NETWORK_VARIANTS) / sizeof(NETWORK_VARIANTS[0])
};
