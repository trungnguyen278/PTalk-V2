/**
 * File:    main.cpp
 * Author:  Trung Nguyen
 * GitHub:  https://github.com/trungnguyen278/PTalk-V2
 * Date:    30 Jun 2026
 *
 * Description:
 *  - Part of the PTalk-V2 project
 *  - Written and maintained by Trung Nguyen
 */

#include "AppController.hpp"
#include "config/DeviceProfile.hpp"
#include "system/StateManager.hpp"
#include "Version.hpp"
#include "esp_log.h"

static const char* TAG = "main";

extern "C" void app_main()
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  PTalk V2 - ESP32-S3");
    ESP_LOGI(TAG, "  Version: %s", app_meta::APP_VERSION);
    ESP_LOGI(TAG, "  Model:   %s", app_meta::DEVICE_MODEL);
    ESP_LOGI(TAG, "  Build:   %s", app_meta::BUILD_DATE);
    ESP_LOGI(TAG, "  ID:      %s", getDeviceEfuseID());
    ESP_LOGI(TAG, "========================================");

    auto& app = AppController::instance();

    // DeviceProfile creates and wires all modules
    if (!DeviceProfile::setup(app)) {
        ESP_LOGE(TAG, "DeviceProfile setup FAILED");
        StateManager::instance().setSystemState(state::SystemState::ERROR);
        return;
    }

    // Initialize AppController (sets up event queue + subscriptions)
    if (!app.init()) {
        ESP_LOGE(TAG, "AppController init FAILED");
        StateManager::instance().setSystemState(state::SystemState::ERROR);
        return;
    }

    // Start all modules
    app.start();

    // Mark system as running
    StateManager::instance().setSystemState(state::SystemState::RUNNING);

    ESP_LOGI(TAG, "ESP32-S3 startup complete");
}
