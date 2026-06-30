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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "AppController.hpp"
#include "config/DeviceProfile.hpp"
#include "system/StateManager.hpp"

static const char* TAG = "MAIN";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "PTalk V2 - ESP32-C5 starting...");

    auto& app = AppController::instance();

    if (!DeviceProfile::setup(app)) {
        ESP_LOGE(TAG, "DeviceProfile setup failed");
        return;
    }

    if (!app.init()) {
        ESP_LOGE(TAG, "AppController init failed");
        return;
    }

    app.start();

    StateManager::instance().setSystemState(state::SystemState::RUNNING);

    ESP_LOGI(TAG, "ESP32-C5 startup complete");
}
