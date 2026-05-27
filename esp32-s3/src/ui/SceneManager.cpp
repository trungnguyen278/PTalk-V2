#include "SceneManager.hpp"
#include <cstring>
#include <cstdlib>

static SceneManager s_instance;

SceneManager& SceneManager::instance() { return s_instance; }

void SceneManager::showScene(const char* categoryKey, const char* variantId) {
    const CategoryDef* cat = findCategory(categoryKey);
    if (!cat || cat->variant_count == 0) return;

    const VariantDef* variant = nullptr;
    if (variantId) {
        for (uint8_t i = 0; i < cat->variant_count; i++) {
            if (strcmp(cat->variants[i].id, variantId) == 0) {
                variant = &cat->variants[i];
                break;
            }
        }
    }
    if (!variant) variant = pickVariant(categoryKey);
    if (!variant) return;

    current_category_ = cat;
    current_variant_ = variant;
    elapsed_ms_ = 0;
    scene_active_ = (cat->kind == ContentKind::SCENE);
    idle_active_ = false;
}

void SceneManager::showEmotion(const char* categoryKey, const char* variantId) {
    showScene(categoryKey, variantId);
    scene_active_ = false;
}

void SceneManager::exitScene() {
    scene_active_ = false;
    idle_active_ = true;
    idle_timer_ms_ = 0;
    idle_interval_ms_ = 3000.0f + (float)(rand() % 3000);
}

const VariantDef* SceneManager::pickVariant(const char* categoryKey) {
    const CategoryDef* cat = findCategory(categoryKey);
    if (!cat || cat->variant_count == 0) return nullptr;

    // Filter out recently played
    const VariantDef* candidates[32];
    int count = 0;
    for (uint8_t i = 0; i < cat->variant_count && count < 32; i++) {
        bool recent = false;
        for (int r = 0; r < RECENT_SIZE; r++) {
            if (recent_ids_[r] && strcmp(recent_ids_[r], cat->variants[i].id) == 0) {
                recent = true;
                break;
            }
        }
        if (!recent) candidates[count++] = &cat->variants[i];
    }

    // Fall back to full list if all filtered
    if (count == 0) {
        for (uint8_t i = 0; i < cat->variant_count && count < 32; i++) {
            candidates[count++] = &cat->variants[i];
        }
    }

    const VariantDef* pick = candidates[rand() % count];

    // Record in recency buffer
    recent_ids_[recent_head_] = pick->id;
    recent_head_ = (recent_head_ + 1) % RECENT_SIZE;

    return pick;
}

void SceneManager::tickIdle(float dt_ms) {
    if (scene_active_ || !idle_active_) return;

    idle_timer_ms_ += dt_ms;
    if (idle_timer_ms_ >= idle_interval_ms_) {
        idle_timer_ms_ = 0;
        idle_interval_ms_ = 3000.0f + (float)(rand() % 3000);

        const VariantDef* v = pickVariant("normal");
        if (v) {
            current_category_ = findCategory("normal");
            current_variant_ = v;
            elapsed_ms_ = 0;
        }
    }
}

ColorContext SceneManager::resolveColors() const {
    if (!current_category_ || !current_variant_) {
        return ColorContext::forEmotion(TONE_CYAN);
    }
    ToneId tone = resolveTone(current_category_, current_variant_);
    if (current_category_->kind == ContentKind::SCENE) {
        return ColorContext::forScene(tone);
    }
    return ColorContext::forEmotion(tone);
}

void SceneManager::update(GfxEngine& gfx, float dt_ms) {
    if (!current_variant_ || !current_variant_->render) return;

    elapsed_ms_ += dt_ms;
    float dur = (float)current_variant_->duration_ms;
    float t = (dur > 0) ? fmodf(elapsed_ms_ / dur, 1.0f) : 0.0f;

    ColorContext colors = resolveColors();
    current_variant_->render(gfx, t, colors);
}
