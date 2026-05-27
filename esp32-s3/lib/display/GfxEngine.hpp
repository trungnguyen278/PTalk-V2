#pragma once
#include <cstdint>
#include <cstring>
#include "MathHelpers.hpp"
#include "ColorSystem.hpp"

class DisplayDriver;

class GfxEngine {
public:
    GfxEngine(uint16_t width, uint16_t height);
    ~GfxEngine();

    bool init();

    uint16_t width()  const { return width_; }
    uint16_t height() const { return height_; }
    uint16_t* getFramebuffer() { return fb_; }

    void clear(uint16_t color = COLOR_BG);

    // --- Filled shapes ---
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                  uint16_t color, uint8_t alpha = 255);

    void fillRoundedRect(int16_t x, int16_t y, int16_t w, int16_t h,
                         int16_t rx, uint16_t color, uint8_t alpha = 255);

    void fillCircle(int16_t cx, int16_t cy, int16_t r,
                    uint16_t color, uint8_t alpha = 255);

    void fillEllipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry,
                     uint16_t color, uint8_t alpha = 255);

    void fillTriangle(int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2,
                      uint16_t color, uint8_t alpha = 255);

    // --- Stroked shapes ---
    void strokeRect(int16_t x, int16_t y, int16_t w, int16_t h,
                    uint16_t color, int16_t sw = 1);

    void strokeRoundedRect(int16_t x, int16_t y, int16_t w, int16_t h,
                           int16_t rx, uint16_t color, int16_t sw = 1);

    void strokeCircle(int16_t cx, int16_t cy, int16_t r,
                      uint16_t color, int16_t sw = 1);

    void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                  uint16_t color, int16_t sw = 1, uint8_t alpha = 255);

    void drawDashedLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                        uint16_t color, int16_t sw,
                        int16_t dashLen, int16_t gapLen);

    void drawArc(int16_t cx, int16_t cy, int16_t r,
                 float startRad, float endRad,
                 uint16_t color, int16_t sw = 1, uint8_t alpha = 255);

    void drawQuadBezier(int16_t x0, int16_t y0,
                        int16_t cx, int16_t cy,
                        int16_t x1, int16_t y1,
                        uint16_t color, int16_t sw = 1);

    void drawCubicBezier(int16_t x0, int16_t y0,
                         int16_t cx1, int16_t cy1,
                         int16_t cx2, int16_t cy2,
                         int16_t x1, int16_t y1,
                         uint16_t color, int16_t sw = 1);

    // --- Text ---
    enum class TextAlign : uint8_t { LEFT, CENTER, RIGHT };

    void drawChar(int16_t x, int16_t y, char c, uint16_t color,
                  uint8_t scale = 1, uint8_t alpha = 255);

    void drawText(const char* text, int16_t x, int16_t y,
                  uint16_t color, uint8_t scale = 1,
                  TextAlign align = TextAlign::LEFT, uint8_t alpha = 255);

    int textWidth(const char* text, uint8_t scale = 1);

    // --- High-level helpers matching emotion-core.jsx ---
    void drawEye(int16_t cx, int16_t cy, int16_t w, int16_t h,
                 int16_t rx, float rotDeg, uint16_t color, uint8_t alpha = 255);

    void drawEyes(uint16_t color,
                  int16_t lx = geom::LX, int16_t ly = geom::CY,
                  int16_t lw = geom::EYE_W, int16_t lh = geom::EYE_H,
                  int16_t rx = geom::RX, int16_t ry = geom::CY,
                  int16_t rw = geom::EYE_W, int16_t rh = geom::EYE_H,
                  int16_t cornerR = geom::EYE_RX);

    // --- Transform stack ---
    void pushTransform();
    void popTransform();
    void translate(float dx, float dy);
    void rotate(float angleDeg, float cx, float cy);
    void scale(float sx, float sy);
    void resetTransform();

    // --- Alpha stack ---
    void pushAlpha(uint8_t alpha);
    void popAlpha();

    // --- Flush to display ---
    void flush(DisplayDriver* drv);

    // --- Pixel-level ---
    void setPixel(int16_t x, int16_t y, uint16_t color, uint8_t alpha = 255);
    uint16_t blendRGB565(uint16_t bg, uint16_t fg, uint8_t alpha);

private:
    void hLine(int16_t x, int16_t y, int16_t w, uint16_t color, uint8_t alpha);
    void applyTransform(float& x, float& y) const;
    void applyTransformI(int16_t& x, int16_t& y) const;

    uint16_t* fb_ = nullptr;
    uint16_t width_, height_;

    struct AffineMatrix {
        float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;
    };
    static constexpr int MAX_DEPTH = 8;
    AffineMatrix xform_stack_[MAX_DEPTH];
    int xform_depth_ = 0;
    AffineMatrix xform_;
    bool xform_identity_ = true;

    uint8_t alpha_stack_[MAX_DEPTH];
    int alpha_depth_ = 0;
    uint8_t global_alpha_ = 255;
};
