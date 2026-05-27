#pragma once
#include <cstdint>
#include <cstring>
#include "display/GfxEngine.hpp"
#include "display/ColorSystem.hpp"
#include "display/MathHelpers.hpp"
#include "ui/VariantRegistry.hpp"

struct SceneData {
    // Time (NTP via C5)
    uint8_t  hours = 0;
    uint8_t  minutes = 0;
    uint8_t  seconds = 0;
    uint8_t  day_of_week = 0;
    uint8_t  day = 1;
    uint8_t  month = 1;
    uint16_t year = 2026;
    bool     time_valid = false;

    // Weather (API via C5)
    int16_t  weather_temp_c = 0;
    uint8_t  weather_condition = 0;
    char     weather_desc[16] = {};
    bool     weather_valid = false;

    // Boot
    uint8_t  boot_progress_pct = 0;
    uint8_t  boot_check_results[5] = {};

    // Network
    char     ssid[33] = {};
    uint8_t  retry_attempt = 0;
    uint8_t  retry_max = 3;
    uint16_t error_code = 0;
};

class SceneManager {
public:
    SceneManager() = default;

    void showScene(const char* categoryKey, const char* variantId = nullptr);
    void showEmotion(const char* categoryKey, const char* variantId = nullptr);
    void exitScene();

    bool isSceneActive() const { return scene_active_; }

    void updateSceneData(const SceneData& data) { scene_data_ = data; }
    SceneData& getSceneData() { return scene_data_; }
    const SceneData& getSceneData() const { return scene_data_; }

    // Call every frame with delta time in ms
    void update(GfxEngine& gfx, float dt_ms);

    // Pick random variant from a category (with recency filter)
    const VariantDef* pickVariant(const char* categoryKey);

    // Idle loop management
    void tickIdle(float dt_ms);
    bool isIdle() const { return !scene_active_ && idle_active_; }

    // Current state
    const VariantDef* currentVariant() const { return current_variant_; }
    const CategoryDef* currentCategory() const { return current_category_; }

    static SceneManager& instance();

private:
    ColorContext resolveColors() const;

    const VariantDef* current_variant_ = nullptr;
    const CategoryDef* current_category_ = nullptr;
    float elapsed_ms_ = 0;
    bool scene_active_ = false;
    bool idle_active_ = true;

    // Recency filter
    static constexpr int RECENT_SIZE = 6;
    const char* recent_ids_[RECENT_SIZE] = {};
    int recent_head_ = 0;

    // Idle timer
    float idle_timer_ms_ = 0;
    float idle_interval_ms_ = 4000;

    SceneData scene_data_;
};
