#include "NetworkManager.hpp"
#include "WifiService.hpp"
#include "WebSocketClient.hpp"
#include "MqttClient.hpp"
#include "SpiBridge.hpp"
#include "UartBridge.hpp"
#include "UartProtocol.hpp"
#include "BluetoothService.hpp"
#include "Version.hpp"

#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
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

void NetworkManager::setSpiBridge(SpiBridge* bridge)   { spi_bridge_ = bridge; }
void NetworkManager::setUartBridge(UartBridge* bridge) { uart_bridge_ = bridge; }

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
                if (speaking_session_active_) {
                    endSpeakingSession();
                    StateManager::instance().setInteractionState(
                        state::InteractionState::IDLE, state::InputSource::SERVER_COMMAND);
                }
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

            // Deferred MQTT: start in a background task after 10s delay
            // so the TCP connect attempt doesn't disrupt the WS connection
            if (!mqtt_ && !cfg_.mqtt_url.empty() && !mqtt_init_pending_) {
                startMqttDeferred();
            }
            break;

        case 0: // CLOSED
            ESP_LOGW(TAG, "WebSocket disconnected");
            ws_connected_ = false;
            if (speaking_session_active_) {
                ESP_LOGW(TAG, "WS disconnected mid-stream, ending speaking session");
                endSpeakingSession();
                StateManager::instance().setInteractionState(
                    state::InteractionState::IDLE, state::InputSource::SERVER_COMMAND);
            }
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

    // Server binary -> relay raw bytes to S3 via SPI audio downlink stream.
    // Data is concatenated [2-byte LE len][opus data] frames. We push raw bytes
    // into SpiBridge's stream buffer — S3 parses [2B len][opus] from its own
    // stream buffer, which handles frame boundaries transparently.
    ws_->onBinary([this](const uint8_t* data, size_t len) {
        if (!data || len == 0 || !spi_bridge_) return;

        static uint32_t bin_msg_count = 0;
        bin_msg_count++;
        if (bin_msg_count == 1 || bin_msg_count % 100 == 0) {
            ESP_LOGI(TAG, "WS binary #%lu: %zu bytes", (unsigned long)bin_msg_count, len);
        }

        if (!speaking_session_active_) {
            auto state = StateManager::instance().getInteractionState();
            if (state == state::InteractionState::PROCESSING ||
                state == state::InteractionState::SPEAKING) {
                startSpeakingSession();
                StateManager::instance().setInteractionState(
                    state::InteractionState::SPEAKING, state::InputSource::SERVER_COMMAND);
            } else {
                return;
            }
        }

        spi_bridge_->sendAudioDownlink(data, len);
    });
}

// === MQTT setup ===

void NetworkManager::mqttPublishResponse(const char* cmd, const char* status, const char* extra_json)
{
    if (!mqtt_) {
        ESP_LOGW(TAG, "MQTT TX skipped (client destroyed): cmd=%s status=%s", cmd, status);
        return;
    }
    std::string topic = "devices/" + std::string(getDeviceEfuseID()) + "/status";
    char buf[256];
    if (extra_json) {
        snprintf(buf, sizeof(buf),
            "{\"cmd\":\"%s\",\"status\":\"%s\",\"device_id\":\"%s\",%s}",
            cmd, status, getDeviceEfuseID(), extra_json);
    } else {
        snprintf(buf, sizeof(buf),
            "{\"cmd\":\"%s\",\"status\":\"%s\",\"device_id\":\"%s\"}",
            cmd, status, getDeviceEfuseID());
    }
    ESP_LOGI(TAG, "MQTT TX: topic=%s", topic.c_str());
    bool ok = mqtt_->publish(topic, buf);
    ESP_LOGI(TAG, "MQTT TX result: %s payload=%s", ok ? "OK" : "FAIL", buf);
}

bool NetworkManager::nvsSetU8(const char* key, uint8_t val)
{
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_u8(h, key, val);
    if (err == ESP_OK) nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

bool NetworkManager::nvsSetString(const char* key, const char* val)
{
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_str(h, key, val);
    if (err == ESP_OK) nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

void NetworkManager::setupMqtt()
{
    mqtt_->onConnected([this]() {
        ESP_LOGI(TAG, "MQTT connected");
        std::string cmd_topic = "devices/" + std::string(getDeviceEfuseID()) + "/cmd";
        mqtt_->subscribe(cmd_topic);
    });

    mqtt_->onMessage([this](const std::string& topic, const std::string& payload) {
        ESP_LOGI(TAG, "MQTT RX: topic=%s", topic.c_str());
        ESP_LOGI(TAG, "MQTT RX: payload=%.*s", (int)payload.size(), payload.c_str());

        cJSON* root = cJSON_Parse(payload.c_str());
        if (!root) {
            ESP_LOGE(TAG, "MQTT RX: JSON parse FAILED");
            return;
        }

        cJSON* cmd_item = cJSON_GetObjectItem(root, "cmd");
        if (!cmd_item || !cJSON_IsString(cmd_item)) {
            ESP_LOGW(TAG, "MQTT RX: missing or invalid 'cmd' field");
            cJSON_Delete(root);
            return;
        }

        std::string cmd = cmd_item->valuestring;
        ESP_LOGI(TAG, "MQTT CMD: '%s'", cmd.c_str());

        // ----- set_volume -----
        if (cmd == "set_volume") {
            cJSON* vol = cJSON_GetObjectItem(root, "volume");
            if (!vol || !cJSON_IsNumber(vol)) {
                mqttPublishResponse("set_volume", "error", "\"message\":\"missing volume param\"");
            } else {
                uint8_t v = (uint8_t)vol->valueint;
                nvsSetU8("volume", v);

                if (uart_bridge_) {
                    uint8_t frame_payload[] = { (uint8_t)uart_proto::ControlCmd::SET_VOLUME, v };
                    uint8_t frame[uart_proto::MAX_FRAME_SIZE];
                    size_t len = uart_proto::buildFrame(frame, uart_proto::MsgType::CONTROL_CMD,
                                                        frame_payload, sizeof(frame_payload));
                    if (len > 0) {
                        uart_write_bytes(UART_NUM_1, frame, len);
                        ESP_LOGI(TAG, "UART TX: SET_VOLUME %d -> S3", v);
                    }
                }

                char extra[32];
                snprintf(extra, sizeof(extra), "\"volume\":%d", v);
                mqttPublishResponse("set_volume", "ok", extra);
            }

        // ----- set_brightness -----
        } else if (cmd == "set_brightness") {
            cJSON* br = cJSON_GetObjectItem(root, "brightness");
            if (!br || !cJSON_IsNumber(br)) {
                mqttPublishResponse("set_brightness", "error", "\"message\":\"missing brightness param\"");
            } else {
                uint8_t b = (uint8_t)br->valueint;
                nvsSetU8("brightness", b);

                if (uart_bridge_) {
                    uint8_t frame_payload[] = { (uint8_t)uart_proto::ControlCmd::SET_BRIGHTNESS, b };
                    uint8_t frame[uart_proto::MAX_FRAME_SIZE];
                    size_t len = uart_proto::buildFrame(frame, uart_proto::MsgType::CONTROL_CMD,
                                                        frame_payload, sizeof(frame_payload));
                    if (len > 0) {
                        uart_write_bytes(UART_NUM_1, frame, len);
                        ESP_LOGI(TAG, "UART TX: SET_BRIGHTNESS %d -> S3", b);
                    }
                }

                char extra[40];
                snprintf(extra, sizeof(extra), "\"brightness\":%d", b);
                mqttPublishResponse("set_brightness", "ok", extra);
            }

        // ----- set_device_name -----
        } else if (cmd == "set_device_name") {
            cJSON* name = cJSON_GetObjectItem(root, "device_name");
            if (!name || !cJSON_IsString(name)) {
                mqttPublishResponse("set_device_name", "error", "\"message\":\"missing device_name param\"");
            } else {
                nvsSetString("device_name", name->valuestring);

                char extra[96];
                snprintf(extra, sizeof(extra), "\"device_name\":\"%s\"", name->valuestring);
                mqttPublishResponse("set_device_name", "ok", extra);
            }

        // ----- set_emotion -----
        } else if (cmd == "set_emotion") {
            cJSON* code = cJSON_GetObjectItem(root, "code");
            if (!code || !cJSON_IsString(code)) {
                mqttPublishResponse("set_emotion", "error", "\"message\":\"missing code param\"");
            } else {
                auto emotion = parseEmotionCode(code->valuestring);
                StateManager::instance().setEmotionState(emotion);

                char extra[48];
                snprintf(extra, sizeof(extra), "\"code\":\"%s\"", code->valuestring);
                mqttPublishResponse("set_emotion", "ok", extra);
            }

        // ----- reboot -----
        } else if (cmd == "reboot") {
            mqttPublishResponse("reboot", "ok", "\"message\":\"rebooting\"");
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();

        // ----- request_status -----
        } else if (cmd == "request_status") {
            auto& sm = StateManager::instance();
            std::string resp_topic = "devices/" + std::string(getDeviceEfuseID()) + "/status";
            char buf[300];
            snprintf(buf, sizeof(buf),
                "{\"cmd\":\"request_status\",\"status\":\"ok\","
                "\"device_id\":\"%s\",\"firmware_version\":\"%s\","
                "\"connectivity\":%d,\"interaction\":%d,"
                "\"free_heap\":%lu,\"uptime_ms\":%lu}",
                getDeviceEfuseID(), app_meta::APP_VERSION,
                (int)sm.getConnectivityState(), (int)sm.getInteractionState(),
                (unsigned long)esp_get_free_heap_size(),
                (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS));
            ESP_LOGI(TAG, "MQTT TX: topic=%s", resp_topic.c_str());
            bool ok = mqtt_->publish(resp_topic, buf);
            ESP_LOGI(TAG, "MQTT TX result: %s payload=%s", ok ? "OK" : "FAIL", buf);

        // ----- request_ble_config -----
        } else if (cmd == "request_ble_config") {
            if (ble_config_active_) {
                mqttPublishResponse("request_ble_config", "ok", "\"message\":\"BLE config already active\"");
            } else {
                startBleConfigMode();
                mqttPublishResponse("request_ble_config", "ok", "\"message\":\"BLE config mode started\"");
            }

        // ----- set_wifi -----
        } else if (cmd == "set_wifi") {
            mqttPublishResponse("set_wifi", "not_supported", "\"message\":\"Use BLE provisioning\"");

        // ----- set_ws_url -----
        } else if (cmd == "set_ws_url") {
            mqttPublishResponse("set_ws_url", "not_supported", "\"message\":\"Use BLE provisioning\"");

        // ----- stop_audio -----
        } else if (cmd == "stop_audio") {
            endSpeakingSession();
            StateManager::instance().setInteractionState(
                state::InteractionState::IDLE, state::InputSource::SERVER_COMMAND);
            mqttPublishResponse("stop_audio", "ok");

        // ----- unknown -----
        } else {
            ESP_LOGW(TAG, "MQTT CMD unknown: '%s'", cmd.c_str());
            char extra[96];
            snprintf(extra, sizeof(extra), "\"message\":\"unknown command: %s\"", cmd.c_str());
            mqttPublishResponse(cmd.c_str(), "invalid_command", extra);
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

void NetworkManager::waitTxDrain(uint32_t timeout_ms)
{
    if (ws_) ws_->waitTxDrain(timeout_ms);
}

size_t NetworkManager::getWsTxFreeSpace() const
{
    return ws_ ? ws_->getTxFreeSpace() : 0;
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

// === Deferred MQTT init ===

void NetworkManager::startMqttDeferred()
{
    mqtt_init_pending_ = true;
    // One-shot task: waits 10s for WS to stabilize, then starts MQTT.
    // Runs on Core 0 at low priority so it doesn't interfere with WS or SPI.
    xTaskCreatePinnedToCore(&NetworkManager::mqttInitTaskEntry, "mqtt_init",
                            3072, this, 1, nullptr, 0);
}

void NetworkManager::mqttInitTaskEntry(void* arg)
{
    auto* self = static_cast<NetworkManager*>(arg);

    // Wait 10s — let WS connection fully stabilize
    vTaskDelay(pdMS_TO_TICKS(10000));

    // Only proceed if WS is still connected and MQTT wasn't started yet
    if (!self->ws_connected_ || self->mqtt_) {
        ESP_LOGI(TAG, "MQTT deferred init cancelled (ws=%d mqtt=%d)",
                 (int)self->ws_connected_, self->mqtt_ != nullptr);
        self->mqtt_init_pending_ = false;
        vTaskDelete(nullptr);
        return;
    }

    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < 20000) {
        ESP_LOGW(TAG, "MQTT skipped - low heap (%lu)", (unsigned long)free_heap);
        self->mqtt_init_pending_ = false;
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "Deferred MQTT init (free heap: %lu)", (unsigned long)free_heap);
    self->mqtt_ = std::make_unique<MqttClient>();
    self->mqtt_->init();
    self->setupMqtt();
    self->mqtt_->setUri(self->cfg_.mqtt_url);
    std::string user = self->cfg_.user_id.empty()
                           ? std::string(getDeviceEfuseID())
                           : self->cfg_.user_id;
    self->mqtt_->setCredentials(user, self->cfg_.tx_key);
    self->mqtt_->start();

    self->mqtt_init_pending_ = false;
    vTaskDelete(nullptr);
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

// === BLE Config Mode (runtime, triggered by MQTT) ===

void NetworkManager::startBleConfigMode()
{
    if (ble_config_active_) return;
    ble_config_active_ = true;

    xTaskCreatePinnedToCore(&NetworkManager::bleConfigTaskEntry, "ble_cfg",
                            4096, this, 2, nullptr, 0);
}

void NetworkManager::stopBleConfigMode()
{
    if (!ble_config_active_) return;
    if (ble_) {
        ble_->deinit();
        delete ble_;
        ble_ = nullptr;
    }
    ble_config_active_ = false;
    ESP_LOGI(TAG, "BLE config mode stopped");
}

void NetworkManager::bleConfigTaskEntry(void* arg)
{
    auto* self = static_cast<NetworkManager*>(arg);

    // The "BLE config mode started" MQTT response was already published by
    // the caller before this task was created. Give it time to flush.
    vTaskDelay(pdMS_TO_TICKS(300));

    // Tear down MQTT and WebSocket to free heap for BLE stack.
    // WS tx_buffer alone is 48KB; together with MQTT they free ~65KB.
    // Safe because BLE config always ends with esp_restart().
    ESP_LOGI(TAG, "BLE config: releasing MQTT & WS to free heap (pre-cleanup: %lu)",
             (unsigned long)esp_get_free_heap_size());

    if (self->mqtt_) {
        self->mqtt_->stop();
        self->mqtt_.reset();
    }
    if (self->ws_) {
        self->ws_->close();
        self->ws_.reset();
    }
    self->ws_connected_ = false;
    vTaskDelay(pdMS_TO_TICKS(300));

    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "BLE config task start, free heap: %lu (after cleanup)", (unsigned long)free_heap);

    if (free_heap < 50000) {
        ESP_LOGW(TAG, "Still not enough heap for BLE (%lu), rebooting", (unsigned long)free_heap);
        self->ble_config_active_ = false;
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
        return;
    }

    // Scan WiFi networks to provide list via BLE
    wifi_scan_config_t scan_cfg = {};
    esp_wifi_scan_start(&scan_cfg, true);
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    std::vector<wifi_ap_record_t> ap_records(ap_count);
    esp_wifi_scan_get_ap_records(&ap_count, ap_records.data());

    std::vector<WifiInfo> networks;
    for (int i = 0; i < ap_count; i++) {
        WifiInfo info;
        info.ssid = (const char*)ap_records[i].ssid;
        info.rssi = ap_records[i].rssi;
        networks.push_back(info);
    }
    ESP_LOGI(TAG, "BLE config: scanned %d WiFi networks", (int)networks.size());

    // Prepare current config to pre-fill BLE characteristics
    BluetoothService::ConfigData current_cfg;
    {
        nvs_handle_t h;
        if (nvs_open("storage", NVS_READONLY, &h) == ESP_OK) {
            size_t len = 0;
            char buf[128];

            len = sizeof(buf);
            if (nvs_get_str(h, "device_name", buf, &len) == ESP_OK) current_cfg.device_name = buf;
            len = sizeof(buf);
            if (nvs_get_str(h, "ws_url", buf, &len) == ESP_OK) current_cfg.ws_url = buf;
            len = sizeof(buf);
            if (nvs_get_str(h, "mqtt_url", buf, &len) == ESP_OK) current_cfg.mqtt_url = buf;
            len = sizeof(buf);
            if (nvs_get_str(h, "user_id", buf, &len) == ESP_OK) current_cfg.mqtt_user = buf;
            len = sizeof(buf);
            if (nvs_get_str(h, "tx_key", buf, &len) == ESP_OK) current_cfg.mqtt_pass = buf;

            nvs_get_u8(h, "volume", &current_cfg.volume);
            nvs_get_u8(h, "brightness", &current_cfg.brightness);
            nvs_close(h);
        }
    }

    // Init BLE
    self->ble_ = new BluetoothService();
    if (!self->ble_->init("PTalk", networks, &current_cfg)) {
        ESP_LOGE(TAG, "BLE init failed, rebooting to restore MQTT/WS");
        delete self->ble_;
        self->ble_ = nullptr;
        self->ble_config_active_ = false;
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
        return;
    }

    static volatile bool config_received = false;
    static BluetoothService::ConfigData received_cfg;
    config_received = false;

    self->ble_->onConfigComplete([](const BluetoothService::ConfigData& cfg) {
        received_cfg = cfg;
        config_received = true;
    });

    self->ble_->start();
    ESP_LOGI(TAG, "BLE advertising started, waiting for config (timeout 120s)...");

    // Wait for config or timeout (120 seconds)
    constexpr int BLE_CONFIG_TIMEOUT_MS = 120000;
    for (int ms = 0; !config_received && ms < BLE_CONFIG_TIMEOUT_MS; ms += 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Teardown BLE
    self->ble_->deinit();
    delete self->ble_;
    self->ble_ = nullptr;
    self->ble_config_active_ = false;

    if (!config_received) {
        ESP_LOGW(TAG, "BLE config timeout, rebooting to restore MQTT/WS");
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
        return;
    }

    ESP_LOGI(TAG, "BLE config received: ssid='%s'", received_cfg.ssid.c_str());

    // Save to NVS
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

    ESP_LOGI(TAG, "BLE config saved, rebooting to apply");
    vTaskDelay(pdMS_TO_TICKS(500));

    // Reboot to apply new config (WiFi/MQTT/WS URLs may have changed)
    esp_restart();
}
