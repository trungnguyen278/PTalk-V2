#include "GfxEngine.hpp"
#include "DisplayDriver.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>
#include "esp_heap_caps.h"

// ─────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────

GfxEngine::GfxEngine(uint16_t width, uint16_t height)
    : width_(width), height_(height) {}

GfxEngine::~GfxEngine() {
    if (fb_) heap_caps_free(fb_);
}

bool GfxEngine::init() {
    size_t sz = width_ * height_ * sizeof(uint16_t);
    fb_ = (uint16_t*)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
    if (!fb_) return false;
    clear(COLOR_BG);
    return true;
}

void GfxEngine::clear(uint16_t color) {
    if (!fb_) return;
    size_t count = width_ * height_;
    if (color == 0) {
        memset(fb_, 0, count * 2);
    } else {
        for (size_t i = 0; i < count; i++) fb_[i] = color;
    }
}

void GfxEngine::flush(DisplayDriver* drv) {
    if (!fb_ || !drv) return;
    drv->setWindow(0, 0, width_ - 1, height_ - 1);
    drv->writePixels(fb_, width_ * height_ * 2);
}

// ─────────────────────────────────────────────────────────
// Pixel-level
// ─────────────────────────────────────────────────────────

uint16_t GfxEngine::blendRGB565(uint16_t bg, uint16_t fg, uint8_t alpha) {
    if (alpha >= 255) return fg;
    if (alpha == 0)   return bg;
    uint8_t inv = 255 - alpha;
    uint32_t rb_bg = bg & 0xF81F;
    uint32_t g_bg  = bg & 0x07E0;
    uint32_t rb_fg = fg & 0xF81F;
    uint32_t g_fg  = fg & 0x07E0;
    uint32_t rb = ((rb_fg * alpha + rb_bg * inv + 0x0801 * 128) >> 8) & 0xF81F;
    uint32_t g  = ((g_fg  * alpha + g_bg  * inv + 0x0020 * 128) >> 8) & 0x07E0;
    return (uint16_t)(rb | g);
}

void GfxEngine::setPixel(int16_t x, int16_t y, uint16_t color, uint8_t alpha) {
    if (!fb_) return;

    if (!xform_identity_) {
        float fx = x, fy = y;
        applyTransform(fx, fy);
        x = (int16_t)roundf(fx);
        y = (int16_t)roundf(fy);
    }

    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;

    uint8_t a = (global_alpha_ == 255) ? alpha :
                (uint8_t)((uint16_t)alpha * global_alpha_ / 255);

    size_t idx = y * width_ + x;
    if (a >= 255) {
        fb_[idx] = color;
    } else if (a > 0) {
        fb_[idx] = blendRGB565(fb_[idx], color, a);
    }
}

void GfxEngine::hLine(int16_t x, int16_t y, int16_t w, uint16_t color, uint8_t alpha) {
    if (!fb_ || y < 0 || y >= height_ || w <= 0) return;

    if (x < 0) { w += x; x = 0; }
    if (x + w > width_) w = width_ - x;
    if (w <= 0) return;

    uint8_t a = (global_alpha_ == 255) ? alpha :
                (uint8_t)((uint16_t)alpha * global_alpha_ / 255);

    uint16_t* row = fb_ + y * width_ + x;
    if (a >= 255) {
        for (int16_t i = 0; i < w; i++) row[i] = color;
    } else if (a > 0) {
        for (int16_t i = 0; i < w; i++) {
            row[i] = blendRGB565(row[i], color, a);
        }
    }
}

// ─────────────────────────────────────────────────────────
// Transform stack
// ─────────────────────────────────────────────────────────

void GfxEngine::applyTransform(float& x, float& y) const {
    float ox = x, oy = y;
    x = xform_.a * ox + xform_.c * oy + xform_.tx;
    y = xform_.b * ox + xform_.d * oy + xform_.ty;
}

void GfxEngine::applyTransformI(int16_t& x, int16_t& y) const {
    float fx = x, fy = y;
    applyTransform(fx, fy);
    x = (int16_t)roundf(fx);
    y = (int16_t)roundf(fy);
}

void GfxEngine::pushTransform() {
    if (xform_depth_ < MAX_DEPTH) {
        xform_stack_[xform_depth_++] = xform_;
    }
}

void GfxEngine::popTransform() {
    if (xform_depth_ > 0) {
        xform_ = xform_stack_[--xform_depth_];
        xform_identity_ = (xform_.a == 1 && xform_.b == 0 &&
                           xform_.c == 0 && xform_.d == 1 &&
                           xform_.tx == 0 && xform_.ty == 0);
    }
}

void GfxEngine::translate(float dx, float dy) {
    xform_.tx += xform_.a * dx + xform_.c * dy;
    xform_.ty += xform_.b * dx + xform_.d * dy;
    xform_identity_ = false;
}

void GfxEngine::rotate(float angleDeg, float cx, float cy) {
    float rad = angleDeg * (math::PI / 180.0f);
    float cs = cosf(rad), sn = sinf(rad);

    // translate to origin, rotate, translate back
    translate(cx, cy);
    AffineMatrix rot;
    rot.a = cs;  rot.c = -sn;
    rot.b = sn;  rot.d = cs;
    rot.tx = 0;  rot.ty = 0;

    AffineMatrix combined;
    combined.a  = xform_.a * rot.a + xform_.c * rot.b;
    combined.b  = xform_.b * rot.a + xform_.d * rot.b;
    combined.c  = xform_.a * rot.c + xform_.c * rot.d;
    combined.d  = xform_.b * rot.c + xform_.d * rot.d;
    combined.tx = xform_.tx;
    combined.ty = xform_.ty;
    xform_ = combined;

    translate(-cx, -cy);
    xform_identity_ = false;
}

void GfxEngine::scale(float sx, float sy) {
    xform_.a *= sx; xform_.c *= sy;
    xform_.b *= sx; xform_.d *= sy;
    xform_identity_ = false;
}

void GfxEngine::resetTransform() {
    xform_ = AffineMatrix{};
    xform_depth_ = 0;
    xform_identity_ = true;
}

// ─────────────────────────────────────────────────────────
// Alpha stack
// ─────────────────────────────────────────────────────────

void GfxEngine::pushAlpha(uint8_t alpha) {
    if (alpha_depth_ < MAX_DEPTH) {
        alpha_stack_[alpha_depth_++] = global_alpha_;
        global_alpha_ = (uint8_t)((uint16_t)global_alpha_ * alpha / 255);
    }
}

void GfxEngine::popAlpha() {
    if (alpha_depth_ > 0) {
        global_alpha_ = alpha_stack_[--alpha_depth_];
    }
}

// ─────────────────────────────────────────────────────────
// fillRect
// ─────────────────────────────────────────────────────────

void GfxEngine::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                          uint16_t color, uint8_t alpha) {
    if (xform_identity_) {
        // Fast path: axis-aligned, no rotation
        int16_t x0 = std::max<int16_t>(x, 0);
        int16_t y0 = std::max<int16_t>(y, 0);
        int16_t x1 = std::min<int16_t>(x + w, width_);
        int16_t y1 = std::min<int16_t>(y + h, height_);
        if (x0 >= x1 || y0 >= y1) return;
        for (int16_t row = y0; row < y1; row++) {
            hLine(x0, row, x1 - x0, color, alpha);
        }
    } else {
        // Slow path: transform each pixel
        for (int16_t dy = 0; dy < h; dy++) {
            for (int16_t dx = 0; dx < w; dx++) {
                setPixel(x + dx, y + dy, color, alpha);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────
// fillRoundedRect — scanline fill with circle-quadrant corners
// ─────────────────────────────────────────────────────────

void GfxEngine::fillRoundedRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                 int16_t rx, uint16_t color, uint8_t alpha) {
    if (w <= 0 || h <= 0) return;
    rx = std::min<int16_t>(rx, std::min<int16_t>(w / 2, h / 2));
    if (rx <= 0) { fillRect(x, y, w, h, color, alpha); return; }

    if (xform_identity_) {
        // Fast scanline path
        for (int16_t row = 0; row < h; row++) {
            int16_t inset = 0;
            if (row < rx) {
                // Top corner region
                int16_t dy = rx - row - 1;
                float offset = (float)rx - sqrtf((float)(rx * rx - dy * dy));
                inset = (int16_t)offset;
            } else if (row >= h - rx) {
                // Bottom corner region
                int16_t dy = row - (h - rx);
                float offset = (float)rx - sqrtf((float)(rx * rx - dy * dy));
                inset = (int16_t)offset;
            }
            int16_t sx = x + inset;
            int16_t sw = w - 2 * inset;
            if (sw > 0) hLine(sx, y + row, sw, color, alpha);
        }
    } else {
        // Transformed: per-pixel
        for (int16_t row = 0; row < h; row++) {
            int16_t inset = 0;
            if (row < rx) {
                int16_t dy = rx - row - 1;
                float offset = (float)rx - sqrtf((float)(rx * rx - dy * dy));
                inset = (int16_t)offset;
            } else if (row >= h - rx) {
                int16_t dy = row - (h - rx);
                float offset = (float)rx - sqrtf((float)(rx * rx - dy * dy));
                inset = (int16_t)offset;
            }
            for (int16_t col = inset; col < w - inset; col++) {
                setPixel(x + col, y + row, color, alpha);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────
// fillCircle — midpoint circle, scanline fill
// ─────────────────────────────────────────────────────────

void GfxEngine::fillCircle(int16_t cx, int16_t cy, int16_t r,
                            uint16_t color, uint8_t alpha) {
    if (r <= 0) { setPixel(cx, cy, color, alpha); return; }

    if (xform_identity_) {
        for (int16_t dy = -r; dy <= r; dy++) {
            int16_t halfW = (int16_t)sqrtf((float)(r * r - dy * dy));
            hLine(cx - halfW, cy + dy, halfW * 2 + 1, color, alpha);
        }
    } else {
        for (int16_t dy = -r; dy <= r; dy++) {
            int16_t halfW = (int16_t)sqrtf((float)(r * r - dy * dy));
            for (int16_t dx = -halfW; dx <= halfW; dx++) {
                setPixel(cx + dx, cy + dy, color, alpha);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────
// fillEllipse
// ─────────────────────────────────────────────────────────

void GfxEngine::fillEllipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry,
                             uint16_t color, uint8_t alpha) {
    if (rx <= 0 || ry <= 0) { setPixel(cx, cy, color, alpha); return; }

    for (int16_t dy = -ry; dy <= ry; dy++) {
        float ratio = 1.0f - ((float)(dy * dy)) / ((float)(ry * ry));
        if (ratio < 0) continue;
        int16_t halfW = (int16_t)(sqrtf(ratio) * rx);
        if (xform_identity_) {
            hLine(cx - halfW, cy + dy, halfW * 2 + 1, color, alpha);
        } else {
            for (int16_t dx = -halfW; dx <= halfW; dx++) {
                setPixel(cx + dx, cy + dy, color, alpha);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────
// fillTriangle — scanline
// ─────────────────────────────────────────────────────────

void GfxEngine::fillTriangle(int16_t x0, int16_t y0,
                              int16_t x1, int16_t y1,
                              int16_t x2, int16_t y2,
                              uint16_t color, uint8_t alpha) {
    // Sort by y
    if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
    if (y0 > y2) { std::swap(x0, x2); std::swap(y0, y2); }
    if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }

    int16_t totalH = y2 - y0;
    if (totalH == 0) return;

    for (int16_t row = y0; row <= y2; row++) {
        bool upper = row < y1 || y1 == y2;
        int16_t segH = upper ? (y1 - y0) : (y2 - y1);
        if (segH == 0) segH = 1;

        float tA = (float)(row - y0) / totalH;
        float tB = upper ? (float)(row - y0) / segH
                         : (float)(row - y1) / segH;

        int16_t xA = (int16_t)(x0 + (x2 - x0) * tA);
        int16_t xB = upper ? (int16_t)(x0 + (x1 - x0) * tB)
                           : (int16_t)(x1 + (x2 - x1) * tB);

        if (xA > xB) std::swap(xA, xB);
        if (xform_identity_) {
            hLine(xA, row, xB - xA + 1, color, alpha);
        } else {
            for (int16_t dx = xA; dx <= xB; dx++) {
                setPixel(dx, row, color, alpha);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────
// Stroked shapes
// ─────────────────────────────────────────────────────────

void GfxEngine::strokeRect(int16_t x, int16_t y, int16_t w, int16_t h,
                            uint16_t color, int16_t sw) {
    fillRect(x, y, w, sw, color);           // top
    fillRect(x, y + h - sw, w, sw, color);  // bottom
    fillRect(x, y, sw, h, color);           // left
    fillRect(x + w - sw, y, sw, h, color);  // right
}

void GfxEngine::strokeRoundedRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                   int16_t rx, uint16_t color, int16_t sw) {
    // Outer minus inner
    fillRoundedRect(x, y, w, h, rx, color);
    int16_t innerRx = std::max<int16_t>(rx - sw, 0);
    fillRoundedRect(x + sw, y + sw, w - 2 * sw, h - 2 * sw, innerRx, COLOR_BG);
}

void GfxEngine::strokeCircle(int16_t cx, int16_t cy, int16_t r,
                              uint16_t color, int16_t sw) {
    int16_t outer = r;
    int16_t inner = r - sw;
    if (inner < 0) inner = 0;

    for (int16_t dy = -outer; dy <= outer; dy++) {
        int16_t outerHW = (int16_t)sqrtf((float)(outer * outer - dy * dy));
        int16_t innerHW = (fabsf((float)dy) <= inner)
            ? (int16_t)sqrtf((float)(inner * inner - dy * dy))
            : 0;

        if (xform_identity_) {
            // Left arc segment
            hLine(cx - outerHW, cy + dy, outerHW - innerHW, color, 255);
            // Right arc segment
            hLine(cx + innerHW + 1, cy + dy, outerHW - innerHW, color, 255);
        } else {
            for (int16_t dx = -outerHW; dx < -innerHW; dx++)
                setPixel(cx + dx, cy + dy, color);
            for (int16_t dx = innerHW + 1; dx <= outerHW; dx++)
                setPixel(cx + dx, cy + dy, color);
        }
    }
}

// ─────────────────────────────────────────────────────────
// drawLine — Bresenham with thickness
// ─────────────────────────────────────────────────────────

void GfxEngine::drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                          uint16_t color, int16_t sw, uint8_t alpha) {
    if (sw <= 1) {
        // Bresenham thin line
        int16_t dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
        int16_t dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
        int16_t err = dx + dy;

        while (true) {
            setPixel(x1, y1, color, alpha);
            if (x1 == x2 && y1 == y2) break;
            int16_t e2 = 2 * err;
            if (e2 >= dy) { err += dy; x1 += sx; }
            if (e2 <= dx) { err += dx; y1 += sy; }
        }
    } else {
        // Thick line: draw perpendicular filled circles along the path
        float dx = x2 - x1, dy = y2 - y1;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.5f) { fillCircle(x1, y1, sw / 2, color, alpha); return; }
        float step = 0.5f;
        int steps = (int)(len / step);
        for (int i = 0; i <= steps; i++) {
            float t = (float)i / steps;
            int16_t px = (int16_t)(x1 + dx * t);
            int16_t py = (int16_t)(y1 + dy * t);
            fillCircle(px, py, sw / 2, color, alpha);
        }
    }
}

void GfxEngine::drawDashedLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                                uint16_t color, int16_t sw,
                                int16_t dashLen, int16_t gapLen) {
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1) return;
    float ux = dx / len, uy = dy / len;

    float pos = 0;
    bool drawing = true;
    while (pos < len) {
        float seg = drawing ? dashLen : gapLen;
        float end = std::min(pos + seg, len);
        if (drawing) {
            drawLine((int16_t)(x1 + ux * pos), (int16_t)(y1 + uy * pos),
                     (int16_t)(x1 + ux * end), (int16_t)(y1 + uy * end),
                     color, sw);
        }
        pos = end;
        drawing = !drawing;
    }
}

// ─────────────────────────────────────────────────────────
// drawArc — stroking an arc segment via stepping
// ─────────────────────────────────────────────────────────

void GfxEngine::drawArc(int16_t cx, int16_t cy, int16_t r,
                         float startRad, float endRad,
                         uint16_t color, int16_t sw, uint8_t alpha) {
    float sweep = endRad - startRad;
    int steps = std::max(8, (int)(fabsf(sweep) * r * 0.5f));
    int16_t prevX = cx + (int16_t)(cosf(startRad) * r);
    int16_t prevY = cy + (int16_t)(sinf(startRad) * r);

    for (int i = 1; i <= steps; i++) {
        float angle = startRad + sweep * i / steps;
        int16_t nx = cx + (int16_t)(cosf(angle) * r);
        int16_t ny = cy + (int16_t)(sinf(angle) * r);
        drawLine(prevX, prevY, nx, ny, color, sw, alpha);
        prevX = nx; prevY = ny;
    }
}

// ─────────────────────────────────────────────────────────
// Bezier curves
// ─────────────────────────────────────────────────────────

void GfxEngine::drawQuadBezier(int16_t x0, int16_t y0,
                                int16_t cx, int16_t cy,
                                int16_t x1, int16_t y1,
                                uint16_t color, int16_t sw) {
    int steps = 16;
    int16_t prevX = x0, prevY = y0;
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / steps;
        float u = 1.0f - t;
        float px = u * u * x0 + 2 * u * t * cx + t * t * x1;
        float py = u * u * y0 + 2 * u * t * cy + t * t * y1;
        int16_t nx = (int16_t)roundf(px), ny = (int16_t)roundf(py);
        drawLine(prevX, prevY, nx, ny, color, sw);
        prevX = nx; prevY = ny;
    }
}

void GfxEngine::drawCubicBezier(int16_t x0, int16_t y0,
                                 int16_t cx1, int16_t cy1,
                                 int16_t cx2, int16_t cy2,
                                 int16_t x1, int16_t y1,
                                 uint16_t color, int16_t sw) {
    int steps = 20;
    int16_t prevX = x0, prevY = y0;
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / steps;
        float u = 1.0f - t;
        float px = u*u*u*x0 + 3*u*u*t*cx1 + 3*u*t*t*cx2 + t*t*t*x1;
        float py = u*u*u*y0 + 3*u*u*t*cy1 + 3*u*t*t*cy2 + t*t*t*y1;
        int16_t nx = (int16_t)roundf(px), ny = (int16_t)roundf(py);
        drawLine(prevX, prevY, nx, ny, color, sw);
        prevX = nx; prevY = ny;
    }
}

// ─────────────────────────────────────────────────────────
// Text — uses Font8x8 (scaled)
// ─────────────────────────────────────────────────────────

#include "Font8x8.hpp"

void GfxEngine::drawChar(int16_t x, int16_t y, char c, uint16_t color,
                          uint8_t scale, uint8_t alpha) {
    if (c < 32 || c > 126) c = '?';
    const uint8_t* glyph = FONT8x8[c - 32];

    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << col)) {
                if (scale == 1) {
                    setPixel(x + col, y + row, color, alpha);
                } else {
                    fillRect(x + col * scale, y + row * scale,
                             scale, scale, color, alpha);
                }
            }
        }
    }
}

void GfxEngine::drawText(const char* text, int16_t x, int16_t y,
                          uint16_t color, uint8_t scale,
                          TextAlign align, uint8_t alpha) {
    if (!text) return;
    int len = strlen(text);
    int charW = 8 * scale;
    int totalW = len * charW;

    int16_t startX = x;
    if (align == TextAlign::CENTER) startX = x - totalW / 2;
    else if (align == TextAlign::RIGHT) startX = x - totalW;

    for (int i = 0; i < len; i++) {
        drawChar(startX + i * charW, y, text[i], color, scale, alpha);
    }
}

int GfxEngine::textWidth(const char* text, uint8_t scale) {
    if (!text) return 0;
    return strlen(text) * 8 * scale;
}

// ─────────────────────────────────────────────────────────
// High-level: Eye / Eyes (matching emotion-core.jsx)
// ─────────────────────────────────────────────────────────

void GfxEngine::drawEye(int16_t cx, int16_t cy, int16_t w, int16_t h,
                         int16_t rx, float rotDeg, uint16_t color, uint8_t alpha) {
    if (h < 1) h = 1;
    int16_t clampedRx = std::min<int16_t>(rx, std::min<int16_t>(w / 2, h / 2));

    if (fabsf(rotDeg) > 0.5f) {
        pushTransform();
        rotate(rotDeg, cx, cy);
    }

    fillRoundedRect(cx - w / 2, cy - h / 2, w, h, clampedRx, color, alpha);

    if (fabsf(rotDeg) > 0.5f) {
        popTransform();
    }
}

void GfxEngine::drawEyes(uint16_t color,
                          int16_t lx, int16_t ly, int16_t lw, int16_t lh,
                          int16_t rx, int16_t ry, int16_t rw, int16_t rh,
                          int16_t cornerR) {
    drawEye(lx, ly, lw, lh, cornerR, 0, color);
    drawEye(rx, ry, rw, rh, cornerR, 0, color);
}
