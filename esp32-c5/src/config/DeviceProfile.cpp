#include "DeviceProfile.hpp"
#include "AppController.hpp"

// System managers
#include "system/AudioManager.hpp"
#include "system/NetworkManager.hpp"
#include "system/UartBridge.hpp"
#include "system/BluetoothService.hpp"
#include "system/StateManager.hpp"
#include "system/StateTypes.hpp"

// Drivers
#include "I2SAudioInput_ICS43434.hpp"
#include "I2SAudioOutput_MAX98357.hpp"
#include "TouchInput.hpp"

// Codec - use interface for easy switching
#include "AudioCodec.hpp"
#include "OpusCodec.hpp"
#include "AdpcmCodec.hpp"

// Pin config
#include "config/PinConfig.hpp"

#include "WifiService.hpp"
#include "driver/i2s_std.h"
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
        uint8_t  volume = 60;
        std::string wifi_ssid;
        std::string wifi_pass;
        std::string ws_url;
        std::string mqtt_url;
        std::string user_id;
        std::string tx_key;
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
        cfg.wifi_ssid = get_string(h, "ssid");
        cfg.wifi_pass = get_string(h, "pass");
        cfg.ws_url    = get_string(h, "ws_url");
        cfg.mqtt_url  = get_string(h, "mqtt_url");
        cfg.user_id   = get_string(h, "user_id");
        cfg.tx_key    = get_string(h, "tx_key");
        cfg.volume    = get_u8(h, "volume", cfg.volume);
        nvs_close(h);
        return cfg;
    }
}

// URL normalization helpers
static std::string normalize_ws_url(std::string val) {
    while (!val.empty() && (val.front() == ' ' || val.front() == '\n')) val.erase(val.begin());
    while (!val.empty() && (val.back() == ' ' || val.back() == '\n')) val.pop_back();
    if (val.empty()) return {};
    if (val.rfind("ws://", 0) == 0 || val.rfind("wss://", 0) == 0) {
        if (val.find('/', val.find("://") + 3) == std::string::npos) val += "/ws";
        return val;
    }
    return "ws://" + val + "/ws";
}

static std::string normalize_mqtt_url(std::string val) {
    while (!val.empty() && (val.front() == ' ' || val.front() == '\n')) val.erase(val.begin());
    while (!val.empty() && (val.back() == ' ' || val.back() == '\n')) val.pop_back();
    if (val.empty()) return {};
    if (val.rfind("mqtt://", 0) == 0 || val.rfind("mqtts://", 0) == 0) return val;
    return "mqtt://" + val;
}

// =============================================================================
// Setup
// =============================================================================
bool DeviceProfile::setup(AppController& app)
{
    ESP_LOGI(TAG, "DeviceProfile setup begin (ESP32-C5)");

    // Init NVS
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    auto user = user_cfg::load();

    // =========================================================
    // 1. I2S Full-Duplex Setup (shared BCLK/WS for mic+speaker)
    // =========================================================
    i2s_chan_handle_t tx_chan = nullptr;
    i2s_chan_handle_t rx_chan = nullptr;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        (i2s_port_t)PinConfig::I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num  = 6;
    chan_cfg.dma_frame_num = 256;

    esp_err_t err = i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S channel creation failed: %s", esp_err_to_name(err));
        return false;
    }

    // =========================================================
    // 2. AUDIO
    // =========================================================
    auto audio_mgr = std::make_unique<AudioManager>();

    // Mic: ICS43434 on RX channel
    I2SAudioInput_ICS43434::Config mic_cfg{
        .pin_bclk    = (gpio_num_t)PinConfig::I2S_BCLK,
        .pin_ws      = (gpio_num_t)PinConfig::I2S_WS,
        .pin_din     = (gpio_num_t)PinConfig::I2S_DIN,
        .sample_rate = 16000
    };
    auto mic = std::make_unique<I2SAudioInput_ICS43434>(rx_chan, mic_cfg);

    // Speaker: MAX98357 on TX channel
    I2SAudioOutput_MAX98357::Config spk_cfg{
        .pin_bclk    = (gpio_num_t)PinConfig::I2S_BCLK,
        .pin_ws      = (gpio_num_t)PinConfig::I2S_WS,
        .pin_dout    = (gpio_num_t)PinConfig::I2S_DOUT,
        .sample_rate = 16000
    };
    auto speaker = std::make_unique<I2SAudioOutput_MAX98357>(tx_chan, spk_cfg);
    speaker->setVolume(user.volume);

    // Codec: Opus (primary) or ADPCM (fallback)
    // To switch codec, change this line:
    #ifdef USE_ADPCM_CODEC
        auto codec = std::make_unique<AdpcmCodec>();
        ESP_LOGI(TAG, "Using ADPCM codec");
    #else
        auto codec = std::make_unique<OpusCodec>();
        ESP_LOGI(TAG, "Using Opus codec");
    #endif

    audio_mgr->setInput(std::move(mic));
    audio_mgr->setOutput(std::move(speaker));
    audio_mgr->setCodec(std::move(codec));

    if (!audio_mgr->init()) {
        ESP_LOGE(TAG, "AudioManager init failed");
        return false;
    }

    // =========================================================
    // 3. BLE PROVISIONING (if no WiFi credentials in NVS)
    // =========================================================
    if (user.wifi_ssid.empty()) {
        ESP_LOGI(TAG, "No WiFi credentials found - starting BLE provisioning");

        // Init WiFi stack briefly to scan networks for BLE listing
        WifiService wifi_scan;
        wifi_scan.init();
        wifi_scan.ensureStaStarted();
        vTaskDelay(pdMS_TO_TICKS(500));
        auto networks = wifi_scan.scanNetworks();
        wifi_scan.disconnect();
        ESP_LOGI(TAG, "Scanned %d WiFi networks for BLE listing", (int)networks.size());

        // Prepare current config for BLE restore
        BluetoothService::ConfigData ble_cfg;
        ble_cfg.device_name = user.device_name;
        ble_cfg.volume = user.volume;
        ble_cfg.ws_url = user.ws_url;
        ble_cfg.mqtt_url = user.mqtt_url;
        ble_cfg.mqtt_user = user.user_id;
        ble_cfg.mqtt_pass = user.tx_key;

        static BluetoothService ble;
        if (!ble.init("PTalk", networks, &ble_cfg)) {
            ESP_LOGE(TAG, "BLE init failed");
            return false;
        }

        // Block until config is received via BLE
        static volatile bool ble_config_received = false;
        static BluetoothService::ConfigData received_cfg;

        ble.onConfigComplete([](const BluetoothService::ConfigData& cfg) {
            received_cfg = cfg;
            ble_config_received = true;
            ESP_LOGI(TAG, "BLE config received: ssid='%s'", cfg.ssid.c_str());
        });

        ble.start();
        ESP_LOGI(TAG, "Waiting for BLE provisioning...");

        while (!ble_config_received) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        ble.stop();

        // Save all received config to NVS
        nvs_handle_t h;
        if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
            if (!received_cfg.ssid.empty())
                nvs_set_str(h, "ssid", received_cfg.ssid.c_str());
            if (!received_cfg.pass.empty())
                nvs_set_str(h, "pass", received_cfg.pass.c_str());
            if (!received_cfg.ws_url.empty())
                nvs_set_str(h, "ws_url", received_cfg.ws_url.c_str());
            if (!received_cfg.mqtt_url.empty())
                nvs_set_str(h, "mqtt_url", received_cfg.mqtt_url.c_str());
            if (!received_cfg.mqtt_user.empty())
                nvs_set_str(h, "user_id", received_cfg.mqtt_user.c_str());
            if (!received_cfg.mqtt_pass.empty())
                nvs_set_str(h, "tx_key", received_cfg.mqtt_pass.c_str());
            if (!received_cfg.device_name.empty())
                nvs_set_str(h, "device_name", received_cfg.device_name.c_str());
            nvs_set_u8(h, "volume", received_cfg.volume);
            nvs_commit(h);
            nvs_close(h);
        }

        // Update user settings with received values
        user.wifi_ssid = received_cfg.ssid;
        user.wifi_pass = received_cfg.pass;
        if (!received_cfg.ws_url.empty()) user.ws_url = received_cfg.ws_url;
        if (!received_cfg.mqtt_url.empty()) user.mqtt_url = received_cfg.mqtt_url;
        if (!received_cfg.mqtt_user.empty()) user.user_id = received_cfg.mqtt_user;
        if (!received_cfg.mqtt_pass.empty()) user.tx_key = received_cfg.mqtt_pass;
        user.device_name = received_cfg.device_name;
        user.volume = received_cfg.volume;

        ESP_LOGI(TAG, "BLE provisioning complete, continuing with WiFi connect");
    }

    // =========================================================
    // 4. NETWORK (WiFi + WebSocket + MQTT)
    // =========================================================
    auto network_mgr = std::make_unique<NetworkManager>();

    const std::string default_ws   = "171.226.10.121:8000";
    const std::string default_mqtt = "171.226.10.121:1883";

    NetworkManager::Config net_cfg{};
    net_cfg.wifi_ssid = user.wifi_ssid;
    net_cfg.wifi_pass = user.wifi_pass;
    net_cfg.ws_url    = normalize_ws_url(user.ws_url.empty() ? default_ws : user.ws_url);
    net_cfg.mqtt_url  = normalize_mqtt_url(user.mqtt_url.empty() ? default_mqtt : user.mqtt_url);
    net_cfg.user_id   = user.user_id;
    net_cfg.tx_key    = user.tx_key;

    if (!network_mgr->init(net_cfg)) {
        ESP_LOGE(TAG, "NetworkManager init failed");
        return false;
    }

    // Wire audio buffers to network
    network_mgr->setMicBuffer(audio_mgr->getMicEncodedBuffer());
    network_mgr->setAudioManager(audio_mgr.get());

    StreamBufferHandle_t spk_sb = audio_mgr->getSpeakerEncodedBuffer();
    NetworkManager* network_ptr = network_mgr.get();

    // Downlink audio: server binary -> speaker buffer
    network_mgr->onServerBinary([spk_sb, network_ptr](const uint8_t* data, size_t len) {
        if (!data || len == 0) return;
        auto interaction = StateManager::instance().getInteractionState();
        if (interaction == state::InteractionState::LISTENING || !network_ptr->isSpeakingSessionActive()) {
            return;
        }
        xStreamBufferSend(spk_sb, data, len, pdMS_TO_TICKS(100));
    });

    // WS disconnect handler
    network_mgr->onDisconnect([spk_sb]() {
        ESP_LOGW(TAG, "WS disconnected - cleanup");
        xStreamBufferReset(spk_sb);
        auto& sm = StateManager::instance();
        if (sm.getInteractionState() == state::InteractionState::SPEAKING) {
            sm.setInteractionState(state::InteractionState::IDLE, state::InputSource::SYSTEM);
        }
    });

    // Server text commands -> interaction state
    network_mgr->onServerText([network_ptr](const std::string& msg) {
        auto& sm = StateManager::instance();
        if (msg == "PROCESSING_START" || msg == "PROCESSING") {
            sm.setInteractionState(state::InteractionState::PROCESSING, state::InputSource::SERVER_COMMAND);
        } else if (msg == "AUDIO_START" || msg == "SPEAKING" || msg == "SPEAK_START") {
            if (!network_ptr->isSpeakingSessionActive()) {
                network_ptr->startSpeakingSession();
                sm.setInteractionState(state::InteractionState::SPEAKING, state::InputSource::SERVER_COMMAND);
            }
        } else if (msg == "IDLE" || msg == "SPEAK_END" || msg == "DONE" || msg == "TTS_END") {
            network_ptr->endSpeakingSession();
            sm.setInteractionState(state::InteractionState::IDLE, state::InputSource::SERVER_COMMAND);
        }
    });

    // Interaction state -> WS control (START/END messages, immune mode)
    static state::InteractionState prev_state = state::InteractionState::IDLE;
    StateManager::instance().subscribeInteraction([network_ptr](state::InteractionState s, state::InputSource) {
        if (s == state::InteractionState::LISTENING && prev_state != state::InteractionState::LISTENING) {
            network_ptr->endSpeakingSession();
            network_ptr->sendText("START");
        } else if (s != state::InteractionState::LISTENING && prev_state == state::InteractionState::LISTENING) {
            network_ptr->sendText("END");
        }
        network_ptr->setWSImmuneMode(s == state::InteractionState::SPEAKING);
        prev_state = s;
    });

    // =========================================================
    // 5. TOUCH INPUT (Button)
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
    // 6. UART BRIDGE (Communication with S3)
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

    // UART: receive WiFi config from S3
    uart_bridge->onWifiConfig([&network_mgr](const std::string& ssid, const std::string& pass) {
        ESP_LOGI(TAG, "UART: WiFi config from S3: ssid='%s'", ssid.c_str());
        // Save to NVS
        nvs_handle_t h;
        if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
            nvs_set_str(h, "ssid", ssid.c_str());
            nvs_set_str(h, "pass", pass.c_str());
            nvs_commit(h);
            nvs_close(h);
        }
        network_mgr->setCredentials(ssid, pass);
    });

    // UART: receive MQTT config from S3
    uart_bridge->onMqttConfig([](const std::string& uri, const std::string& user, const std::string& pass) {
        ESP_LOGI(TAG, "UART: MQTT config from S3: uri='%s'", uri.c_str());
        nvs_handle_t h;
        if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
            nvs_set_str(h, "mqtt_url", uri.c_str());
            nvs_set_str(h, "user_id", user.c_str());
            nvs_set_str(h, "tx_key", pass.c_str());
            nvs_commit(h);
            nvs_close(h);
        }
    });

    // UART: receive volume command from S3
    AudioManager* audio_ptr = audio_mgr.get();
    uart_bridge->onCommand([audio_ptr, &app](uart_proto::Command cmd, const uint8_t* data, size_t len) {
        switch (cmd) {
        case uart_proto::Command::REBOOT:
            app.reboot();
            break;
        case uart_proto::Command::SET_VOLUME:
            if (len >= 1 && audio_ptr) audio_ptr->setVolume(data[0]);
            break;
        default:
            break;
        }
    });

    // Forward emotion state to S3 via UART for display
    UartBridge* uart_ptr = uart_bridge.get();
    StateManager::instance().subscribeEmotion([uart_ptr](state::EmotionState e) {
        uart_ptr->sendDisplayCmd((uint8_t)e);
    });

    // =========================================================
    // 7. ATTACH MODULES -> APP CONTROLLER
    // =========================================================
    app.attachModules(
        std::move(audio_mgr),
        std::move(network_mgr),
        std::move(touch_input),
        std::move(uart_bridge));

    ESP_LOGI(TAG, "DeviceProfile setup OK (ESP32-C5)");
    return true;
}
