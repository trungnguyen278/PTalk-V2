#pragma once
#include <cstdint>
#include "display/GfxEngine.hpp"
#include "display/ColorSystem.hpp"

using RenderFn = void(*)(GfxEngine& gfx, float t, const ColorContext& colors);

struct VariantDef {
    const char* id;
    const char* label;
    uint16_t duration_ms;
    ToneId tone;
    RenderFn render;
};

struct CategoryDef {
    const char* key;
    const char* label;
    ContentKind kind;
    ToneId tone;
    const VariantDef* variants;
    uint8_t variant_count;
};

ToneId resolveTone(const CategoryDef* cat, const VariantDef* variant);

const CategoryDef* findCategory(const char* key);
const VariantDef* findVariant(const char* id);

// All registered categories (defined in VariantRegistry.cpp)
extern const CategoryDef ALL_CATEGORIES[];
extern const uint8_t CATEGORY_COUNT;
