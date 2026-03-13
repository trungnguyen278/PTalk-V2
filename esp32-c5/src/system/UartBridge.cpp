#include "UartBridge.hpp"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "UartBridge";

UartBridge::~UartBridge()
{
    stop();
}

bool UartBridge::init(const Config& cfg)
{
    cfg_ = cfg;
    uart_port_ = (uart_port_t)cfg_.uart_num;

    uart_config_t uart_cfg = {};
    uart_cfg.baud_rate  = cfg_.baud_rate;
    uart_cfg.data_bits  = UART_DATA_8_BITS;
    uart_cfg.parity     = UART_PARITY_DISABLE;
    uart_cfg.stop_bits  = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    uart_cfg.source_clk = UART_SCLK_DEFAULT;

    esp_err_t err = uart_driver_install(uart_port_, cfg_.rx_buf_size, cfg_.tx_buf_size, 0, nullptr, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(err));
        return false;
    }

    err = uart_param_config(uart_port_, &uart_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART param config failed: %s", esp_err_to_name(err));
        return false;
    }

    err = uart_set_pin(uart_port_, cfg_.tx_pin, cfg_.rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART set pin failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "UART%d initialized (TX=%d, RX=%d, %d baud)",
             uart_port_, cfg_.tx_pin, cfg_.rx_pin, cfg_.baud_rate);
    return true;
}

void UartBridge::start()
{
    if (started_) return;
    started_ = true;

    xTaskCreatePinnedToCore(
        &UartBridge::rxTaskEntry,
        "UartRxTask",
        4096,
        this,
        3,
        &rx_task_,
        0 // Core 0
    );
}

void UartBridge::stop()
{
    if (!started_) return;
    started_ = false;

    if (rx_task_) {
        vTaskDelay(pdMS_TO_TICKS(200));
        rx_task_ = nullptr;
    }
}

// --- TX: Send frames to S3 ---

bool UartBridge::sendFrame(uint8_t type, const uint8_t* payload, uint16_t len)
{
    uint8_t frame[uart_proto::FRAME_OVERHEAD + uart_proto::MAX_PAYLOAD];
    size_t frame_len = uart_proto::buildFrame(frame, sizeof(frame), type, payload, len);
    if (frame_len == 0) return false;

    int written = uart_write_bytes(uart_port_, frame, frame_len);
    return written == (int)frame_len;
}

bool UartBridge::sendStatusUpdate(uint8_t interaction, uint8_t connectivity, uint8_t system_state)
{
    uint8_t payload[3] = { interaction, connectivity, system_state };
    return sendFrame(static_cast<uint8_t>(uart_proto::MsgToS3::STATUS_UPDATE), payload, 3);
}

bool UartBridge::sendDisplayCmd(uint8_t emotion)
{
    return sendFrame(static_cast<uint8_t>(uart_proto::MsgToS3::DISPLAY_CMD), &emotion, 1);
}

bool UartBridge::sendConfigRequest()
{
    return sendFrame(static_cast<uint8_t>(uart_proto::MsgToS3::CONFIG_REQ), nullptr, 0);
}

// --- RX: Receive frames from S3 ---

void UartBridge::rxTaskEntry(void* arg)
{
    static_cast<UartBridge*>(arg)->rxTaskLoop();
}

void UartBridge::rxTaskLoop()
{
    ESP_LOGI(TAG, "UART RX task started");

    uart_proto::FrameParser parser;
    uint8_t byte;

    while (started_) {
        int len = uart_read_bytes(uart_port_, &byte, 1, pdMS_TO_TICKS(50));
        if (len <= 0) continue;

        auto result = parser.feed(byte);
        switch (result) {
        case uart_proto::FrameParser::Result::FRAME_READY:
            handleFrame(parser.msgType(), parser.payload(), parser.payloadLen());
            parser.reset();
            break;
        case uart_proto::FrameParser::Result::ERROR:
            ESP_LOGW(TAG, "Frame parse error, resetting");
            parser.reset();
            break;
        case uart_proto::FrameParser::Result::NEED_MORE:
            break;
        }
    }

    ESP_LOGW(TAG, "UART RX task ended");
    vTaskDelete(nullptr);
}

void UartBridge::handleFrame(uint8_t type, const uint8_t* payload, uint16_t len)
{
    auto msg = static_cast<uart_proto::MsgFromS3>(type);

    switch (msg) {
    case uart_proto::MsgFromS3::WIFI_CONFIG: {
        if (len < 2) break;
        size_t pos = 0;
        uint8_t ssid_len = payload[pos++];
        if (pos + ssid_len > len) break;
        std::string ssid(reinterpret_cast<const char*>(&payload[pos]), ssid_len);
        pos += ssid_len;
        if (pos >= len) break;
        uint8_t pass_len = payload[pos++];
        if (pos + pass_len > len) break;
        std::string pass(reinterpret_cast<const char*>(&payload[pos]), pass_len);
        ESP_LOGI(TAG, "WiFi config received: ssid='%s'", ssid.c_str());
        if (wifi_cb_) wifi_cb_(ssid, pass);
        break;
    }
    case uart_proto::MsgFromS3::MQTT_CONFIG: {
        if (len < 3) break;
        size_t pos = 0;
        auto readStr = [&](std::string& out) -> bool {
            if (pos >= len) return false;
            uint8_t slen = payload[pos++];
            if (pos + slen > len) return false;
            out.assign(reinterpret_cast<const char*>(&payload[pos]), slen);
            pos += slen;
            return true;
        };
        std::string uri, user, pass;
        if (readStr(uri) && readStr(user) && readStr(pass)) {
            ESP_LOGI(TAG, "MQTT config received: uri='%s'", uri.c_str());
            if (mqtt_cb_) mqtt_cb_(uri, user, pass);
        }
        break;
    }
    case uart_proto::MsgFromS3::WS_CONFIG: {
        if (len < 1) break;
        uint8_t url_len = payload[0];
        if (1 + url_len > len) break;
        std::string url(reinterpret_cast<const char*>(&payload[1]), url_len);
        ESP_LOGI(TAG, "WS config received: url='%s'", url.c_str());
        if (ws_cb_) ws_cb_(url);
        break;
    }
    case uart_proto::MsgFromS3::ADC_DATA: {
        if (len < 4) break;
        uint16_t voltage_mv = payload[0] | ((uint16_t)payload[1] << 8);
        uint8_t battery_pct = payload[2];
        bool charging = payload[3] != 0;
        if (adc_cb_) adc_cb_(voltage_mv, battery_pct, charging);
        break;
    }
    case uart_proto::MsgFromS3::COMMAND: {
        if (len < 1) break;
        auto cmd = static_cast<uart_proto::Command>(payload[0]);
        if (cmd_cb_) cmd_cb_(cmd, &payload[1], len - 1);
        break;
    }
    case uart_proto::MsgFromS3::ACK:
        ESP_LOGD(TAG, "ACK received from S3");
        break;
    default:
        ESP_LOGW(TAG, "Unknown message type: 0x%02X", type);
        break;
    }
}
