#include "NetworkManager.hpp"
#include "WifiService.hpp"
#include "WebSocketClient.hpp"
#include "MqttClient.hpp"
#include "AudioManager.hpp"
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
    // MQTT is lazy-initialized: created only after WS is connected
    // to avoid wasting ~9KB RAM and blocking esp-tls during WS setup

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

    // If we have credentials, connect directly; otherwise autoConnect loads from NVS
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
    uplink_task_ = nullptr;

    ESP_LOGI(TAG, "NetworkManager stopped");
}

void NetworkManager::setCredentials(const std::string& ssid, const std::string& pass)
{
    cfg_.wifi_ssid = ssid;
    cfg_.wifi_pass = pass;
    if (wifi_) wifi_->connectWithCredentials(ssid.c_str(), pass.c_str());
}

void NetworkManager::setMicBuffer(StreamBufferHandle_t mic_buf) { mic_buf_ = mic_buf; }
void NetworkManager::setAudioManager(AudioManager* mgr) { audio_mgr_ = mgr; }

// === WiFi setup ===

void NetworkManager::setupWifi()
{
    wifi_->onStatus([this](int status) {
        switch (status) {
        case 2: // GOT_IP
            ESP_LOGI(TAG, "WiFi connected");
            StateManager::instance().setConnectivityState(state::ConnectivityState::WIFI_CONNECTED);

            // Connect WebSocket
            if (ws_ && !cfg_.ws_url.empty()) {
                StateManager::instance().setConnectivityState(state::ConnectivityState::CONNECTING_WS);
                ws_->setUrl(cfg_.ws_url);
                ws_->connect();
            }

            // MQTT is lazy-started after WS connects (see setupWebSocket)
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

            // Start uplink task
            if (!uplink_task_ && mic_buf_) {
                xTaskCreatePinnedToCore(&NetworkManager::uplinkTaskEntry,
                    "Uplink", 3072, this, 4, &uplink_task_, 0);
            }

            // Lazy MQTT: only start if enough heap (MQTT needs ~9KB, keep 15KB for WiFi/WS)
            if (!mqtt_ && !cfg_.mqtt_url.empty()) {
                size_t free_heap = esp_get_free_heap_size();
                if (free_heap > 35000) {
                    ESP_LOGI(TAG, "Lazy MQTT init (free heap: %lu)", (unsigned long)free_heap);
                    mqtt_ = std::make_unique<MqttClient>();
                    mqtt_->init();
                    setupMqtt();
                    mqtt_->setUri(cfg_.mqtt_url);
                    std::string user = cfg_.user_id.empty() ? std::string(getDeviceEfuseID()) : cfg_.user_id;
                    mqtt_->setCredentials(user, cfg_.tx_key);
                    mqtt_->start();
                } else {
                    ESP_LOGW(TAG, "MQTT skipped - low heap (%lu bytes), need >35KB", (unsigned long)free_heap);
                }
            }
            break;

        case 0: // CLOSED
            ESP_LOGW(TAG, "WebSocket disconnected");
            ws_connected_ = false;
            if (!ws_immune_) {
                StateManager::instance().setConnectivityState(state::ConnectivityState::WIFI_CONNECTED);
            }
            if (disconnect_cb_) disconnect_cb_();
            break;

        case 1: // CONNECTING
            break;
        }
    });

    ws_->onText([this](const std::string& msg) {
        ESP_LOGI(TAG, "WS text: %s", msg.c_str());

        // Check for emotion code prefix (2-digit code before command)
        if (msg.length() >= 2) {
            std::string potential_code = msg.substr(0, 2);
            if (potential_code[0] >= '0' && potential_code[0] <= '9' &&
                potential_code[1] >= '0' && potential_code[1] <= '9') {
                auto emotion = parseEmotionCode(potential_code);
                StateManager::instance().setEmotionState(emotion);
                if (msg.length() > 2) {
                    if (text_cb_) text_cb_(msg.substr(2));
                    return;
                }
            }
        }

        if (text_cb_) text_cb_(msg);
    });

    ws_->onBinary([this](const uint8_t* data, size_t len) {
        if (binary_cb_) binary_cb_(data, len);
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
                if (vol && cJSON_IsNumber(vol) && audio_mgr_) {
                    audio_mgr_->setVolume((uint8_t)vol->valueint);
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

// === Uplink task ===

void NetworkManager::uplinkTaskEntry(void* arg)
{
    static_cast<NetworkManager*>(arg)->uplinkLoop();
}

void NetworkManager::uplinkLoop()
{
    ESP_LOGI(TAG, "Uplink task started");
    // Batch buffer: accumulate multiple length-prefixed Opus frames
    // then send as one WS binary message to reduce overhead
    static uint8_t batch_buf[2048];
    size_t batch_pos = 0;
    uint32_t frame_count = 0;
    static uint32_t send_count = 0;

    while (started_) {
        if (!ws_connected_ ||
            StateManager::instance().getInteractionState() != state::InteractionState::LISTENING) {
            // Flush remaining data before going idle
            if (batch_pos > 0) {
                sendBinary(batch_buf, batch_pos);
                batch_pos = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // Read 2-byte length prefix
        uint8_t header[2];
        size_t got = xStreamBufferReceive(mic_buf_, header, 2, pdMS_TO_TICKS(20));
        if (got < 2) {
            // Timeout — flush whatever we have
            if (batch_pos > 0) {
                sendBinary(batch_buf, batch_pos);
                send_count++;
                if (send_count == 1 || send_count % 50 == 0) {
                    ESP_LOGI(TAG, "Uplink: batch #%lu, %zu bytes, %lu frames",
                             (unsigned long)send_count, batch_pos, (unsigned long)frame_count);
                }
                batch_pos = 0;
                frame_count = 0;
            }
            continue;
        }

        uint16_t frame_len = header[0] | ((uint16_t)header[1] << 8);
        if (frame_len == 0 || frame_len > 256) {
            ESP_LOGE(TAG, "Uplink: bad frame len %u, resetting", frame_len);
            xStreamBufferReset(mic_buf_);
            batch_pos = 0;
            frame_count = 0;
            continue;
        }

        // Check if this frame fits in the batch
        size_t total_frame = 2 + frame_len;
        if (batch_pos + total_frame > sizeof(batch_buf)) {
            // Flush current batch first
            sendBinary(batch_buf, batch_pos);
            send_count++;
            if (send_count == 1 || send_count % 50 == 0) {
                ESP_LOGI(TAG, "Uplink: batch #%lu, %zu bytes, %lu frames",
                         (unsigned long)send_count, batch_pos, (unsigned long)frame_count);
            }
            batch_pos = 0;
            frame_count = 0;
        }

        // Copy header
        batch_buf[batch_pos] = header[0];
        batch_buf[batch_pos + 1] = header[1];

        // Read Opus data
        size_t data_got = xStreamBufferReceive(mic_buf_, batch_buf + batch_pos + 2,
                                                frame_len, pdMS_TO_TICKS(50));
        if (data_got < frame_len) {
            ESP_LOGW(TAG, "Uplink: incomplete frame (%zu/%u)", data_got, frame_len);
            continue;
        }

        batch_pos += total_frame;
        frame_count++;
    }

    ESP_LOGW(TAG, "Uplink task ended");
    vTaskDelete(nullptr);
}
