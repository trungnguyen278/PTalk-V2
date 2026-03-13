#include "DeviceProfile.hpp"
#include "AppController.hpp"

// System managers
#include "system/DisplayManager.hpp"
#include "system/PowerManager.hpp"
#include "system/UartBridge.hpp"
#include "system/StateManager.hpp"
#include "system/StateTypes.hpp"

// Drivers
#include "DisplayDriver.hpp"
#include "Power.hpp"

// Pin config
#include "config/PinConfig.hpp"
#include "Version.hpp"

// Assets
#include "assets/emotions/neutral.hpp"
#include "assets/emotions/idle.hpp"
#include "assets/emotions/listening.hpp"
#include "assets/emotions/happy.hpp"
#include "assets/emotions/sad.hpp"
#include "assets/emotions/thinking.hpp"
#include "assets/emotions/stun.hpp"
#include "assets/icons/battery_charge.hpp"
#include "assets/icons/battery_full.hpp"
#include "assets/icons/critical_power.hpp"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char* TAG = "DeviceProfile";

// =============================================================================
// User settings from NVS
// =============================================================================
namespace user_cfg {
    struct UserSettings {
        std::string device_name = "PTalk";
        uint8_t volume = 60;
        uint8_t brightness = 100;
    };

    static std::string get_string(nvs_handle_t h, const char* key) {
        size_t required = 0;
        if (nvs_get_str(h, key, nullptr, &required) != ESP_OK || required == 0) return {};
        std::string tmp; tmp.resize(required);
        if (nvs_get_str(h, key, tmp.data(), &required) != ESP_OK) return {};
        if (!tmp.empty() && tmp.back() == '\0') tmp.pop_back();
        return tmp;
    }

    static uint8_t get_u8(nvs_handle_t h, const char* key, uint8_t def_val) {
        uint8_t v = def_val;
        nvs_get_u8(h, key, &v);
        return v;
    }

    static UserSettings load() {
        UserSettings cfg;
        nvs_handle_t h;
        if (nvs_open("storage", NVS_READONLY, &h) != ESP_OK) {
            ESP_LOGI(TAG, "NVS storage not found, using defaults");
            return cfg;
        }
        cfg.device_name = get_string(h, "device_name");
        if (cfg.device_name.empty()) cfg.device_name = "PTalk";
        cfg.volume      = get_u8(h, "volume", cfg.volume);
        cfg.brightness  = get_u8(h, "brightness", cfg.brightness);
        nvs_close(h);
        return cfg;
    }
}

// =============================================================================
// Helper: register all emotion animations from assets
// =============================================================================
static void registerEmotions(DisplayManager* display)
{
    auto reg = [&](const char* name, const asset::emotion::Animation& a) {
        Animation1Bit anim1bit;
        anim1bit.width           = a.width;
        anim1bit.height          = a.height;
        anim1bit.frame_count     = a.frame_count;
        anim1bit.fps             = a.fps;
        anim1bit.loop            = a.loop;
        anim1bit.max_packed_size = a.max_packed_size;
        anim1bit.base_frame      = nullptr; // Frame 0 is diff from black
        anim1bit.frames          = a.frames ? a.frames() : nullptr;
        display->registerEmotion(name, anim1bit);
    };

    reg("neutral",   asset::emotion::NEUTRAL);
    reg("idle",      asset::emotion::IDLE);
    reg("listening", asset::emotion::LISTENING);
    reg("happy",     asset::emotion::HAPPY);
    reg("sad",       asset::emotion::SAD);
    reg("thinking",  asset::emotion::THINKING);
    reg("stun",      asset::emotion::STUN);
}

// =============================================================================
// Setup
// =============================================================================
bool DeviceProfile::setup(AppController& app)
{
    ESP_LOGI(TAG, "DeviceProfile setup begin (ESP32-S3)");

    // Init NVS
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    auto user = user_cfg::load();

    // =========================================================
    // 1. DISPLAY (ST7789 240x320 via SPI)
    // =========================================================
    DisplayDriver::Config lcd_cfg{};
    lcd_cfg.spi_host = SPI2_HOST;
    lcd_cfg.pin_cs   = PinConfig::LCD_CS;
    lcd_cfg.pin_dc   = PinConfig::LCD_DC;
    lcd_cfg.pin_rst  = PinConfig::LCD_RST;
    lcd_cfg.pin_bl   = PinConfig::LCD_BL;
    lcd_cfg.pin_mosi = PinConfig::SPI_MOSI;
    lcd_cfg.pin_sclk = PinConfig::SPI_SCLK;

    auto lcd_driver = std::make_unique<DisplayDriver>();
    if (!lcd_driver->init(lcd_cfg)) {
        ESP_LOGE(TAG, "DisplayDriver init failed");
        return false;
    }

    auto display_mgr = std::make_unique<DisplayManager>();
    if (!display_mgr->init(std::move(lcd_driver), 240, 320)) {
        ESP_LOGE(TAG, "DisplayManager init failed");
        return false;
    }

    display_mgr->setBrightness(user.brightness);

    // Register emotion animations
    registerEmotions(display_mgr.get());

    // Enable state binding (display auto-reacts to StateManager)
    display_mgr->enableStateBinding(true);

    // =========================================================
    // 2. POWER (Battery ADC)
    // =========================================================
    auto power_drv = std::make_unique<Power>(
        ADC1_CHANNEL_0, // GPIO1
        (gpio_num_t)PinConfig::CHG_STATUS,
        (gpio_num_t)PinConfig::CHG_FULL,
        PinConfig::BAT_R1,
        PinConfig::BAT_R2
    );

    PowerManager::Config pwr_cfg{};
    pwr_cfg.evaluate_interval_ms = 2000;
    pwr_cfg.critical_percent = 8.0f;
    pwr_cfg.enable_smoothing = true;

    auto power_mgr = std::make_unique<PowerManager>(std::move(power_drv), pwr_cfg);
    if (!power_mgr->init()) {
        ESP_LOGW(TAG, "PowerManager init failed (non-fatal)");
    }

    // Link display for battery % overlay
    power_mgr->setDisplayManager(display_mgr.get());

    // =========================================================
    // 3. UART BRIDGE (Communication with C5)
    // =========================================================
    auto uart_bridge = std::make_unique<UartBridge>();
    UartBridge::Config uart_cfg{
        .uart_num    = PinConfig::UART_NUM,
        .tx_pin      = PinConfig::UART_TX,
        .rx_pin      = PinConfig::UART_RX,
        .baud_rate   = PinConfig::UART_BAUD_RATE,
        .rx_buf_size = 1024,
        .tx_buf_size = 512
    };

    if (!uart_bridge->init(uart_cfg)) {
        ESP_LOGE(TAG, "UartBridge init failed");
        return false;
    }

    // UART: receive status updates from C5 -> update StateManager
    uart_bridge->onStatusUpdate([](uint8_t interaction, uint8_t connectivity, uint8_t system_state) {
        auto& sm = StateManager::instance();
        sm.setInteractionState(static_cast<state::InteractionState>(interaction));
        sm.setConnectivityState(static_cast<state::ConnectivityState>(connectivity));
    });

    // UART: receive display command (emotion) from C5
    uart_bridge->onDisplayCmd([](uint8_t emotion) {
        StateManager::instance().setEmotionState(static_cast<state::EmotionState>(emotion));
    });

    // UART: C5 requests saved config -> S3 no longer stores WiFi/MQTT config (no BLE)
    // Config is managed entirely on C5 side

    // PowerManager: send ADC data to C5 when power state changes
    UartBridge* uart_ptr = uart_bridge.get();
    PowerManager* pwr_ptr = power_mgr.get();
    StateManager::instance().subscribePower([uart_ptr, pwr_ptr](state::PowerState s) {
        uint16_t mv = (uint16_t)(pwr_ptr->getVoltage() * 1000);
        uint8_t pct = pwr_ptr->getPercent();
        bool charging = pwr_ptr->isCharging();
        uart_ptr->sendAdcData(mv, pct, charging);
    });

    // =========================================================
    // 4. ATTACH MODULES -> APP CONTROLLER
    // =========================================================
    app.attachModules(
        std::move(display_mgr),
        std::move(power_mgr),
        std::move(uart_bridge));

    ESP_LOGI(TAG, "DeviceProfile setup OK (ESP32-S3)");
    return true;
}
