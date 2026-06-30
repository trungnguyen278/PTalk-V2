/**
 * File:    DeviceProfile.cpp
 * Author:  Trung Nguyen
 * GitHub:  https://github.com/trungnguyen278/PTalk-V2
 * Date:    30 Jun 2026
 *
 * Description:
 *  - Part of the PTalk-V2 project
 *  - Written and maintained by Trung Nguyen
 */

#include "DeviceProfile.hpp"
#include "AppController.hpp"
#include "esp_log.h"

// System managers
#include "system/DisplayManager.hpp"
#include "system/PowerManager.hpp"
#include "system/AudioManager.hpp"
#include "system/SpiBridge.hpp"
#include "system/UartBridge.hpp"
#include "system/StateManager.hpp"
#include "system/StateTypes.hpp"

// Drivers
#include "DisplayDriver.hpp"
#include "Power.hpp"
#include "I2SAudioInput_ICS43434.hpp"
#include "I2SAudioOutput_MAX98357.hpp"
#include "TouchInput.hpp"

// Codec
#include "AudioCodec.hpp"
#include "OpusCodec.hpp"

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

#include "driver/i2s_std.h"
#include "driver/gpio.h"
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
        anim1bit.base_frame      = nullptr;
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
    ESP_LOGI(TAG, "DeviceProfile setup begin (ESP32-S3), free heap: %lu",
             (unsigned long)esp_get_free_heap_size());

    // Init NVS
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    auto user = user_cfg::load();

    // =========================================================
    // 1. DISPLAY (ST7789 240x320 via SPI2)
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
    registerEmotions(display_mgr.get());
    display_mgr->enableStateBinding(true);

    // =========================================================
    // 2. POWER (Battery ADC)
    // =========================================================
    auto power_drv = std::make_unique<Power>(
        ADC1_CHANNEL_0,
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
    power_mgr->setDisplayManager(display_mgr.get());

    // =========================================================
    // 3. MAX98357 CONTROL PINS
    // =========================================================
    gpio_config_t spk_sd_cfg = {};
    spk_sd_cfg.pin_bit_mask = (1ULL << PinConfig::SPK_SD);
    spk_sd_cfg.mode = GPIO_MODE_OUTPUT;
    spk_sd_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    spk_sd_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&spk_sd_cfg);
    gpio_set_level((gpio_num_t)PinConfig::SPK_SD, 0);

    gpio_config_t spk_gain_cfg = {};
    spk_gain_cfg.pin_bit_mask = (1ULL << PinConfig::SPK_GAIN);
    spk_gain_cfg.mode = GPIO_MODE_OUTPUT;  // Drive LOW (tied to GND) = 12dB
    spk_gain_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    spk_gain_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&spk_gain_cfg);
    gpio_set_level((gpio_num_t)PinConfig::SPK_GAIN, 0);  // LOW = 12dB
    ESP_LOGI(TAG, "MAX98357 SD=LOW (muted until playback), GAIN=LOW (12dB)");

    // =========================================================
    // 4. I2S FULL-DUPLEX AUDIO (shared BCLK/WS for mic+speaker)
    // =========================================================
    i2s_chan_handle_t tx_chan = nullptr;
    i2s_chan_handle_t rx_chan = nullptr;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        (i2s_port_t)PinConfig::I2S_NUM, I2S_ROLE_MASTER);
    // Larger DMA buffer to prevent underrun during Opus decode jitter.
    // 6 descriptors × 480 frames = 2880 samples (~60ms @ 48kHz).
    // With only 4×240 (960 samples = 20ms), a single decode stall causes audible pop.
    chan_cfg.dma_desc_num  = 6;
    chan_cfg.dma_frame_num = 480;

    esp_err_t err = i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S channel creation failed: %s", esp_err_to_name(err));
        return false;
    }

    auto audio_mgr = std::make_unique<AudioManager>();

    I2SAudioInput_ICS43434::Config mic_cfg{
        .pin_bclk    = (gpio_num_t)PinConfig::I2S_BCLK,
        .pin_ws      = (gpio_num_t)PinConfig::I2S_WS,
        .pin_din     = (gpio_num_t)PinConfig::I2S_DIN,
        .sample_rate = 48000
    };
    auto mic = std::make_unique<I2SAudioInput_ICS43434>(rx_chan, mic_cfg);

    I2SAudioOutput_MAX98357::Config spk_cfg{
        .pin_bclk    = (gpio_num_t)PinConfig::I2S_BCLK,
        .pin_ws      = (gpio_num_t)PinConfig::I2S_WS,
        .pin_dout    = (gpio_num_t)PinConfig::I2S_DOUT,
        .pin_sd      = (gpio_num_t)PinConfig::SPK_SD,
        .sample_rate = 48000
    };
    auto speaker = std::make_unique<I2SAudioOutput_MAX98357>(tx_chan, spk_cfg);
    speaker->init();
    speaker->setVolume(user.volume);

    auto codec = std::make_unique<OpusCodec>();
    ESP_LOGI(TAG, "Using Opus codec");

    audio_mgr->setInput(std::move(mic));
    audio_mgr->setOutput(std::move(speaker));
    audio_mgr->setCodec(std::move(codec));

    if (!audio_mgr->init()) {
        ESP_LOGE(TAG, "AudioManager init failed");
        return false;
    }

    i2s_channel_enable(tx_chan);
    ESP_LOGI(TAG, "I2S TX enabled (clock source for full-duplex)");

    // =========================================================
    // 5. SPI BRIDGE (Communication with C5 via SPI3)
    // =========================================================
    auto spi_bridge = std::make_unique<SpiBridge>();
    SpiBridge::Config spi_cfg{
        .pin_mosi      = (gpio_num_t)PinConfig::SPI3_MOSI,
        .pin_miso      = (gpio_num_t)PinConfig::SPI3_MISO,
        .pin_sclk      = (gpio_num_t)PinConfig::SPI3_SCLK,
        .pin_cs        = (gpio_num_t)PinConfig::SPI3_CS,
        .pin_handshake = (gpio_num_t)PinConfig::SPI3_HANDSHAKE,
        .clock_speed_hz = 10 * 1000 * 1000  // 10 MHz
    };

    if (!spi_bridge->init(spi_cfg)) {
        ESP_LOGE(TAG, "SpiBridge init failed");
        return false;
    }

    // Wire SpiBridge to AudioManager
    audio_mgr->setSpiBridge(spi_bridge.get());

    // SPI downlink: C5 sends raw audio bytes -> speaker encoded stream buffer.
    // C5 streams raw bytes (up to MAX_PAYLOAD per SPI transaction, NOT frame-aligned).
    // The stream buffer accumulates bytes; AudioRecv parses [2B len][opus] from it.
    // This handles Opus frames of any size (including >250 bytes from server).
    StreamBufferHandle_t spk_sb = audio_mgr->getSpeakerEncodedBuffer();
    spi_bridge->onAudioDownlink([spk_sb](const uint8_t* data, size_t len) {
        if (!data || len == 0) return;
        auto& sm = StateManager::instance();
        auto interaction = sm.getInteractionState();
        if (interaction == state::InteractionState::LISTENING) return;

        // Auto-trigger SPEAKING only from PROCESSING (audio binary arrives before
        // the SPEAKING text command). Do NOT auto-trigger from IDLE — stale SPI data
        // arriving during stopSpeaking() drain would regress the state back to SPEAKING.
        if (interaction == state::InteractionState::PROCESSING) {
            ESP_LOGI("SpiBridge", "Audio downlink auto-trigger SPEAKING (was PROCESSING)");
            sm.setInteractionState(state::InteractionState::SPEAKING, state::InputSource::SERVER_COMMAND);
        } else if (interaction != state::InteractionState::SPEAKING) {
            return;
        }

        // All-or-nothing write to preserve [2B len][opus] stream alignment.
        // Partial writes or dropped SPI chunks corrupt frame boundaries and
        // cause cascading Opus decode errors. Wait for AudioRecv (Core 1)
        // to drain enough space — SPI poll runs on Core 0, no deadlock risk.
        size_t space = xStreamBufferSpacesAvailable(spk_sb);
        if (space >= len) {
            xStreamBufferSend(spk_sb, data, len, 0);
        } else {
            TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(80);
            bool wrote = false;
            while (!wrote) {
                TickType_t now = xTaskGetTickCount();
                if (now >= deadline) break;
                vTaskDelay(pdMS_TO_TICKS(2));
                if (xStreamBufferSpacesAvailable(spk_sb) >= len) {
                    xStreamBufferSend(spk_sb, data, len, 0);
                    wrote = true;
                }
            }
            if (!wrote) {
                static uint32_t dl_drop_count = 0;
                dl_drop_count++;
                if (dl_drop_count <= 5 || dl_drop_count % 50 == 0) {
                    ESP_LOGW("SpiBridge", "DL encoded buffer full after 80ms wait, dropped %zu bytes #%lu",
                             len, (unsigned long)dl_drop_count);
                }
            }
        }
    });

    // =========================================================
    // 6. UART BRIDGE (Control/status with C5 via UART1)
    // =========================================================
    auto uart_bridge = std::make_unique<UartBridge>();
    UartBridge::Config uart_cfg{
        .pin_tx = (gpio_num_t)PinConfig::UART_TX,
        .pin_rx = (gpio_num_t)PinConfig::UART_RX,
    };

    if (!uart_bridge->init(uart_cfg)) {
        ESP_LOGE(TAG, "UartBridge init failed");
        return false;
    }

    // UART status: C5 sends connectivity/emotion state via UART
    uart_bridge->onStatusUpdate([](uint8_t interaction, uint8_t connectivity,
                                    uint8_t system_state, uint8_t emotion) {
        auto& sm = StateManager::instance();

        sm.setConnectivityState(static_cast<state::ConnectivityState>(connectivity));
        sm.setEmotionState(static_cast<state::EmotionState>(emotion));

        // Forward system state from C5, but never revert S3 to BOOTING
        // (S3 manages its own boot lifecycle independently)
        auto new_sys = static_cast<state::SystemState>(system_state);
        if (new_sys != state::SystemState::BOOTING) {
            sm.setSystemState(new_sys);
        }

        // Let C5 drive the interaction state, but don't regress SPEAKING→PROCESSING.
        // UART status carries C5's interaction state at the moment of the emotion change,
        // which can be stale (e.g. emotion fires before SPEAKING command is processed).
        auto new_inter = static_cast<state::InteractionState>(interaction);
        auto cur_inter = sm.getInteractionState();
        if (new_inter == state::InteractionState::SPEAKING) {
            if (cur_inter == state::InteractionState::PROCESSING ||
                cur_inter == state::InteractionState::LISTENING ||
                cur_inter == state::InteractionState::TRIGGERED) {
                sm.setInteractionState(new_inter, state::InputSource::UNKNOWN);
            }
        } else if (new_inter == state::InteractionState::PROCESSING &&
                   cur_inter != state::InteractionState::SPEAKING) {
            sm.setInteractionState(new_inter, state::InputSource::UNKNOWN);
        } else if (new_inter == state::InteractionState::IDLE &&
                   cur_inter == state::InteractionState::SPEAKING) {
            sm.setInteractionState(state::InteractionState::IDLE, state::InputSource::UNKNOWN);
        }
    });

    // UART control commands: C5 forwards MQTT commands (volume, brightness) to S3
    DisplayManager* disp_raw = display_mgr.get();
    AudioManager* audio_raw = audio_mgr.get();
    uart_bridge->onControlCmd([disp_raw, audio_raw](uart_proto::ControlCmd cmd,
                                                      const uint8_t* data, size_t len) {
        switch (cmd) {
        case uart_proto::ControlCmd::SET_VOLUME:
            if (len >= 1 && audio_raw) {
                audio_raw->setVolume(data[0]);
                ESP_LOGI("UartCtrl", "SET_VOLUME %d from C5", data[0]);
            }
            break;
        case uart_proto::ControlCmd::SET_BRIGHTNESS:
            if (len >= 1 && disp_raw) {
                disp_raw->setBrightness(data[0]);
                ESP_LOGI("UartCtrl", "SET_BRIGHTNESS %d from C5", data[0]);
            }
            break;
        default:
            ESP_LOGW("UartCtrl", "Unknown control cmd 0x%02X from C5", (int)cmd);
            break;
        }
    });

    // =========================================================
    // 7. TOUCH INPUT (Button)
    // =========================================================
    auto touch_input = std::make_unique<TouchInput>();
    TouchInput::Config touch_cfg{
        .pin = (gpio_num_t)PinConfig::BUTTON,
        .active_low = true,
        .long_press_ms = 1200
    };

    if (!touch_input->init(touch_cfg)) {
        ESP_LOGE(TAG, "TouchInput init failed");
        return false;
    }

    touch_input->onEvent([&app](TouchInput::Event e) {
        if (e == TouchInput::Event::PRESS)   app.postEvent(event::AppEvent::USER_BUTTON);
        if (e == TouchInput::Event::RELEASE) app.postEvent(event::AppEvent::RELEASE_BUTTON);
    });

    // =========================================================
    // 8. ATTACH MODULES -> APP CONTROLLER
    // =========================================================
    app.attachModules(
        std::move(display_mgr),
        std::move(power_mgr),
        std::move(audio_mgr),
        std::move(spi_bridge),
        std::move(uart_bridge),
        std::move(touch_input));

    ESP_LOGI(TAG, "DeviceProfile setup OK (ESP32-S3)");
    return true;
}
