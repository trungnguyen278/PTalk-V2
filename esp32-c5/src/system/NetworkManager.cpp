#include "NetworkManager.hpp"
#include "WifiService.hpp"
#include "WebSocketClient.hpp"
#include "MqttClient.hpp"
#include "SpiBridge.hpp"
#include "Version.hpp"

#include "esp_log.h"
#include "cJSON.h"
#include <cstring>

static const char* TAG = "NetworkManager";

NetworkManager::NetworkManager() = default;
NetworkManager::~NetworkManager() { stop(); }

bool NetworkManager::init(const Config& cfg)
{
    cfg_ = cfg;
    ESP_LOGI(TAG, "init: ws=%s, mqtt=%s", cfg_.ws_url.c_str(), cfg_.mqtt_url.c_str());

    wifi_ = std::make_unique<WifiService>();
    ws_   = std::make_unique<WebSocketClient>();

    wifi_->init();
    ws_->init();

    setupWifi();
    setupWebSocket();

    return true;
}

void NetworkManager::start()
{
    if (started_) return;
    started_ = true;

    if (!cfg_.wifi_ssid.empty()) {
        wifi_->connectWithCredentials(cfg_.wifi_ssid.c_str(), cfg_.wifi_pass.c_str());
    } else {
        wifi_->autoConnect();
    }

    ESP_LOGI(TAG, "NetworkManager started");
}

void NetworkManager::stop()
{
    if (!started_) return;
    started_ = false;

    if (ws_) ws_->close();
    if (mqtt_) mqtt_->stop();

    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_LOGI(TAG, "NetworkManager stopped");
}

void NetworkManager::setCredentials(const std::string& ssid, const std::string& pass)
{
    cfg_.wifi_ssid = ssid;
    cfg_.wifi_pass = pass;
    if (wifi_) wifi_->connectWithCredentials(ssid.c_str(), pass.c_str());
}

void NetworkManager::setSpiBridge(SpiBridge* bridge) { spi_bridge_ = bridge; }

// === WiFi setup ===

void NetworkManager::setupWifi()
{
    wifi_->onStatus([this](int status) {
        switch (status) {
        case 2: // GOT_IP
            ESP_LOGI(TAG, "WiFi connected");
            StateManager::instance().setConnectivityState(state::ConnectivityState::WIFI_CONNECTED);
            if (ws_ && !cfg_.ws_url.empty()) {
                StateManager::instance().setConnectivityState(state::ConnectivityState::CONNECTING_WS);
                ws_->setUrl(cfg_.ws_url);
                ws_->connect();
            }
            break;
        case 0: // DISCONNECTED
            ESP_LOGW(TAG, "WiFi disconnected");
            if (!ws_immune_) {
                StateManager::instance().setConnectivityState(state::ConnectivityState::OFFLINE);
                ws_connected_ = false;
            }
            break;
        case 1: // CONNECTING
            StateManager::instance().setConnectivityState(state::ConnectivityState::CONNECTING_WIFI);
            break;
        }
    });
}

// === WebSocket setup ===

void NetworkManager::setupWebSocket()
{
    ws_->onStatus([this](int status) {
        switch (status) {
        case 2: // OPEN
            ESP_LOGI(TAG, "WebSocket connected");
            ws_connected_ = true;
            StateManager::instance().setConnectivityState(state::ConnectivityState::ONLINE);
            sendDeviceHandshake();

            // Lazy MQTT
            if (!mqtt_ && !cfg_.mqtt_url.empty()) {
                size_t free_heap = esp_get_free_heap_size();
                if (free_heap > 20000) {
                    ESP_LOGI(TAG, "Lazy MQTT init (free heap: %lu)", (unsigned long)free_heap);
                    mqtt_ = std::make_unique<MqttClient>();
                    mqtt_->init();
                    setupMqtt();
                    mqtt_->setUri(cfg_.mqtt_url);
                    std::string user = cfg_.user_id.empty() ? std::string(getDeviceEfuseID()) : cfg_.user_id;
                    mqtt_->setCredentials(user, cfg_.tx_key);
                    mqtt_->start();
                } else {
                    ESP_LOGW(TAG, "MQTT skipped - low heap (%lu)", (unsigned long)free_heap);
                }
            }
            break;

        case 0: // CLOSED
            ESP_LOGW(TAG, "WebSocket disconnected");
            ws_connected_ = false;
            if (!ws_immune_) {
                StateManager::instance().setConnectivityState(state::ConnectivityState::WIFI_CONNECTED);
            }
            break;

        case 1: // CONNECTING
            break;
        }
    });

    // Server text commands -> parse emotion + relay interaction state to S3 via SPI
    ws_->onText([this](const std::string& msg) {
        ESP_LOGI(TAG, "WS text: %s", msg.c_str());

        std::string command = msg;

        // Check for emotion code prefix
        if (msg.length() >= 2) {
            std::string potential_code = msg.substr(0, 2);
            if (potential_code[0] >= '0' && potential_code[0] <= '9' &&
                potential_code[1] >= '0' && potential_code[1] <= '9') {
                auto emotion = parseEmotionCode(potential_code);
                StateManager::instance().setEmotionState(emotion);
                command = (msg.length() > 2) ? msg.substr(2) : "";
            }
        }

        if (command.empty()) return;

        // Process server commands and relay state to S3 via SPI STATUS_UPDATE
        auto& sm = StateManager::instance();
        if (command == "PROCESSING_START" || command == "PROCESSING") {
            sm.setInteractionState(state::InteractionState::PROCESSING, state::InputSource::SERVER_COMMAND);
        } else if (command == "AUDIO_START" || command == "SPEAKING" || command == "SPEAK_START") {
            if (!speaking_session_active_) {
                startSpeakingSession();
                sm.setInteractionState(state::InteractionState::SPEAKING, state::InputSource::SERVER_COMMAND);
            }
        } else if (command == "IDLE" || command == "SPEAK_END" || command == "DONE" || command == "TTS_END") {
            endSpeakingSession();
            sm.setInteractionState(state::InteractionState::IDLE, state::InputSource::SERVER_COMMAND);
        }
    });

    // Server binary -> relay to S3 via SPI audio downlink
    // Data from server is length-prefixed Opus frames: [2-byte LE len][opus data]...
    // Each SPI frame can carry MAX_PAYLOAD(250) bytes. We must send complete
    // [len][opus] frames and never split a frame across SPI transactions.
    ws_->onBinary([this](const uint8_t* data, size_t len) {
        if (!data || len == 0 || !spi_bridge_) return;

        // Auto-start speaking session if binary arrives during PROCESSING
        // (server may send audio before explicit AUDIO_START text)
        if (!speaking_session_active_) {
            auto state = StateManager::instance().getInteractionState();
            if (state == state::InteractionState::PROCESSING ||
                state == state::InteractionState::SPEAKING) {
                startSpeakingSession();
                StateManager::instance().setInteractionState(
                    state::InteractionState::SPEAKING, state::InputSource::SERVER_COMMAND);
            } else {
                return;  // Not in a state where audio should play
            }
        }

        size_t offset = 0;
        while (offset + 2 <= len) {
            uint16_t frame_len = data[offset] | ((uint16_t)data[offset + 1] << 8);
            size_t total = 2 + frame_len;

            // Validate frame
            if (frame_len == 0 || frame_len > 500 || offset + total > len) {
                ESP_LOGW(TAG, "WS binary: bad frame at offset %zu (frame_len=%u, remaining=%zu)",
                         offset, frame_len, len - offset);
                break;
            }

            // Each complete [len][opus] frame must fit in one SPI payload
            if (total <= spi_proto::MAX_PAYLOAD) {
                spi_bridge_->sendAudioDownlink(data + offset, total);
            } else {
                ESP_LOGW(TAG, "WS binary: frame too large for SPI (%zu > %zu)", total, spi_proto::MAX_PAYLOAD);
            }

            offset += total;
        }
    });
}

// === MQTT setup ===

void NetworkManager::setupMqtt()
{
    mqtt_->onConnected([this]() {
        ESP_LOGI(TAG, "MQTT connected");
        std::string cmd_topic = "devices/" + std::string(getDeviceEfuseID()) + "/cmd";
        mqtt_->subscribe(cmd_topic);
    });

    mqtt_->onMessage([this](const std::string& topic, const std::string& payload) {
        ESP_LOGI(TAG, "MQTT: topic=%s", topic.c_str());

        cJSON* root = cJSON_Parse(payload.c_str());
        if (!root) return;

        cJSON* cmd_item = cJSON_GetObjectItem(root, "cmd");
        if (cmd_item && cJSON_IsString(cmd_item)) {
            std::string cmd = cmd_item->valuestring;

            if (cmd == "set_volume") {
                cJSON* vol = cJSON_GetObjectItem(root, "volume");
                if (vol && cJSON_IsNumber(vol) && spi_bridge_) {
                    uint8_t v = (uint8_t)vol->valueint;
                    spi_bridge_->sendStatusUpdate(
                        (uint8_t)StateManager::instance().getInteractionState(),
                        (uint8_t)StateManager::instance().getConnectivityState(),
                        (uint8_t)StateManager::instance().getSystemState(),
                        (uint8_t)StateManager::instance().getEmotionState());
                }
            } else if (cmd == "reboot") {
                esp_restart();
            } else if (cmd == "request_status") {
                std::string status_topic = "devices/" + std::string(getDeviceEfuseID()) + "/status";
                auto& sm = StateManager::instance();
                char buf[256];
                snprintf(buf, sizeof(buf),
                    "{\"status\":\"ok\",\"device_id\":\"%s\",\"firmware_version\":\"%s\","
                    "\"connectivity\":%d,\"interaction\":%d}",
                    getDeviceEfuseID(), app_meta::APP_VERSION,
                    (int)sm.getConnectivityState(), (int)sm.getInteractionState());
                mqtt_->publish(status_topic, buf);
            }
        }
        cJSON_Delete(root);
    });
}

// === WebSocket operations ===

void NetworkManager::sendText(const std::string& msg)
{
    if (ws_ && ws_connected_) ws_->sendText(msg);
}

void NetworkManager::sendBinary(const uint8_t* data, size_t len)
{
    if (ws_ && ws_connected_) ws_->sendBinary(data, len);
}

void NetworkManager::setWSImmuneMode(bool enable) { ws_immune_ = enable; }

void NetworkManager::sendDeviceHandshake()
{
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"cmd\":\"device_handshake\",\"device_id\":\"%s\","
        "\"firmware_version\":\"%s\",\"device_name\":\"%s\"}",
        getDeviceEfuseID(), app_meta::APP_VERSION, app_meta::DEVICE_MODEL);
    sendText(buf);
}

// === Emotion parsing ===

state::EmotionState NetworkManager::parseEmotionCode(const std::string& code)
{
    if (code == "01") return state::EmotionState::HAPPY;
    if (code == "11") return state::EmotionState::SAD;
    if (code == "02") return state::EmotionState::ANGRY;
    if (code == "12") return state::EmotionState::CONFUSED;
    if (code == "03") return state::EmotionState::EXCITED;
    if (code == "13") return state::EmotionState::CALM;
    return state::EmotionState::NEUTRAL;
}
