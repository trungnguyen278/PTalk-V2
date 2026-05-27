#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace math {

constexpr float PI  = 3.14159265f;
constexpr float TAU = 6.28318530f;

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

inline float clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float wrap(float v, float m) {
    float r = fmodf(v, m);
    return r < 0 ? r + m : r;
}

namespace ease {
    inline float inOut(float t) {
        return t < 0.5f ? 2.0f * t * t : 1.0f - powf(-2.0f * t + 2.0f, 2) / 2.0f;
    }
    inline float out(float t) { return 1.0f - powf(1.0f - t, 3); }
    inline float in(float t) { return t * t * t; }
}

inline float pulse(float t, float center, float w) {
    float d = fabsf(t - center);
    if (d > w) return 0.0f;
    float c = cosf((d / w) * (PI / 2.0f));
    return c * c;
}

inline float blink(float t, float at, float d = 0.06f) {
    float dd = t - at;
    if (dd < 0 || dd > d) return 0.0f;
    return sinf((dd / d) * PI);
}

// 256-entry sine lookup (fixed-point Q15 for fast integer trig)
// sinLUT[i] = sin(i * TAU / 256) * 32767
constexpr int16_t SIN_LUT_SIZE = 256;

inline int16_t sinLUTValue(int idx) {
    // Computed at call site; for constexpr init use a generator
    return (int16_t)(sinf((float)idx / 256.0f * TAU) * 32767.0f);
}

inline float fastSin(float angle) {
    float norm = wrap(angle, TAU) / TAU * 256.0f;
    int idx = (int)norm & 0xFF;
    float frac = norm - (int)norm;
    int next = (idx + 1) & 0xFF;
    float s0 = sinLUTValue(idx) / 32767.0f;
    float s1 = sinLUTValue(next) / 32767.0f;
    return s0 + (s1 - s0) * frac;
}

inline float fastCos(float angle) {
    return fastSin(angle + PI / 2.0f);
}

} // namespace math

// Screen geometry constants (matching ui_design/emotion-core.jsx)
namespace geom {

constexpr int16_t SCREEN_W = 320;
constexpr int16_t SCREEN_H = 240;
constexpr int16_t STATUS_H = 20;
constexpr int16_t CY       = STATUS_H + (SCREEN_H - STATUS_H) / 2; // 130
constexpr int16_t GAP      = 150;
constexpr int16_t LX       = SCREEN_W / 2 - GAP / 2;  // 85
constexpr int16_t RX       = SCREEN_W / 2 + GAP / 2;  // 235
constexpr int16_t EYE_W    = 88;
constexpr int16_t EYE_H    = 96;
constexpr int16_t EYE_RX   = 22;
constexpr int16_t SCX      = SCREEN_W / 2; // 160
constexpr int16_t SCY      = CY;           // 130

}
