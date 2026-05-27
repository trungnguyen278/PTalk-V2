#include "VariantRegistry.hpp"
#include <cstring>

// Category definitions from emotion/scene render files
extern const CategoryDef CAT_NORMAL;
extern const CategoryDef CAT_HAPPY;
extern const CategoryDef CAT_SAD;
extern const CategoryDef CAT_ANGRY;
extern const CategoryDef CAT_SURPRISED;
extern const CategoryDef CAT_LOVE;
extern const CategoryDef CAT_SLEEPY;
extern const CategoryDef CAT_SLEEPING;
extern const CategoryDef CAT_CRYING;
extern const CategoryDef CAT_EXCITED;
extern const CategoryDef CAT_CONFUSED;
extern const CategoryDef CAT_BORED;
extern const CategoryDef CAT_LISTENING;
extern const CategoryDef CAT_THINKING;
extern const CategoryDef CAT_LOADING;
extern const CategoryDef CAT_DIZZY;
extern const CategoryDef CAT_DEAD;
extern const CategoryDef CAT_BOOT;
extern const CategoryDef CAT_NETWORK;
extern const CategoryDef CAT_WEATHER;
extern const CategoryDef CAT_CLOCK;
extern const CategoryDef CAT_MUSIC;
extern const CategoryDef CAT_TIMER;
extern const CategoryDef CAT_COOKING;
extern const CategoryDef CAT_WALKING;
extern const CategoryDef CAT_CELEBRATE;
extern const CategoryDef CAT_NIGHT;
extern const CategoryDef CAT_FITNESS;
extern const CategoryDef CAT_CALL;
extern const CategoryDef CAT_MESSAGE;
extern const CategoryDef CAT_CAMERA;
extern const CategoryDef CAT_NAVIGATION;
extern const CategoryDef CAT_GIFT;
extern const CategoryDef CAT_COFFEE;
extern const CategoryDef CAT_PLANT;
extern const CategoryDef CAT_MORNING;
extern const CategoryDef CAT_GAMING;
extern const CategoryDef CAT_CALENDAR;

const CategoryDef ALL_CATEGORIES[] = {
    CAT_NORMAL,
    CAT_HAPPY,
    CAT_SAD,
    CAT_ANGRY,
    CAT_SURPRISED,
    CAT_LOVE,
    CAT_SLEEPY,
    CAT_SLEEPING,
    CAT_CRYING,
    CAT_EXCITED,
    CAT_CONFUSED,
    CAT_BORED,
    CAT_LISTENING,
    CAT_THINKING,
    CAT_LOADING,
    CAT_DIZZY,
    CAT_DEAD,
    CAT_BOOT,
    CAT_NETWORK,
    CAT_WEATHER,
    CAT_CLOCK,
    CAT_MUSIC,
    CAT_TIMER,
    CAT_COOKING,
    CAT_WALKING,
    CAT_CELEBRATE,
    CAT_NIGHT,
    CAT_FITNESS,
    CAT_CALL,
    CAT_MESSAGE,
    CAT_CAMERA,
    CAT_NAVIGATION,
    CAT_GIFT,
    CAT_COFFEE,
    CAT_PLANT,
    CAT_MORNING,
    CAT_GAMING,
    CAT_CALENDAR,
};

const uint8_t CATEGORY_COUNT = sizeof(ALL_CATEGORIES) / sizeof(ALL_CATEGORIES[0]);

ToneId resolveTone(const CategoryDef* cat, const VariantDef* variant) {
    if (variant && variant->tone != TONE_NONE) return variant->tone;
    if (cat) return cat->tone;
    return TONE_CYAN;
}

const CategoryDef* findCategory(const char* key) {
    for (uint8_t i = 0; i < CATEGORY_COUNT; i++) {
        if (strcmp(ALL_CATEGORIES[i].key, key) == 0)
            return &ALL_CATEGORIES[i];
    }
    return nullptr;
}

const VariantDef* findVariant(const char* id) {
    for (uint8_t i = 0; i < CATEGORY_COUNT; i++) {
        const auto& cat = ALL_CATEGORIES[i];
        for (uint8_t v = 0; v < cat.variant_count; v++) {
            if (strcmp(cat.variants[v].id, id) == 0)
                return &cat.variants[v];
        }
    }
    return nullptr;
}
