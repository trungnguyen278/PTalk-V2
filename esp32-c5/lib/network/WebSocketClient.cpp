#include "WebSocketClient.hpp"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_heap_caps.h"

static const char *TAG = "WebSocketClient";

WebSocketClient::WebSocketClient() = default;

WebSocketClient::~WebSocketClient()
{
    close();
}

void WebSocketClient::init()
{
    if (!tx_buffer) {
        tx_buffer = xStreamBufferCreate(48 * 1024, 1);  // 48KB — room for uplink bursts
    }
}

void WebSocketClient::setUrl(const std::string &url)
{
    ws_url = url;
}

void WebSocketClient::connect()
{
    if (ws_url.empty())
    {
        ESP_LOGE(TAG, "WebSocket URL not set");
        return;
    }

    if (client != nullptr)
    {
        ESP_LOGW(TAG, "WS already created, closing old instance");
        close();
    }

    esp_websocket_client_config_t cfg = {};
    cfg.uri = ws_url.c_str();
    cfg.buffer_size = 8192; // 8KB buffer — C5 has spare heap now (no audio/Opus)
    cfg.disable_auto_reconnect = false;
    cfg.reconnect_timeout_ms = 3000;  // Retry every 3s
    // --- CẤU HÌNH ĐỂ PHÁT HIỆN SERVER NGẮT KẾT NỐI ---
    cfg.ping_interval_sec = 10;     // Cứ 10 giây gửi 1 gói Ping
    cfg.pingpong_timeout_sec = 120; // Chờ 120s mới disconnect để tránh rớt mạng oan khi TCP nghẽn

    // Tùy chọn: Bật TCP Keep-alive để tầng mạng bền bỉ hơn
    cfg.keep_alive_enable = true;
    cfg.keep_alive_idle = 5;     // Thời gian chờ rảnh rỗi (giây)
    cfg.keep_alive_interval = 5; // Khoảng cách giữa các gói probe (giây)
    cfg.keep_alive_count = 3;    // Số lần thử lại trước khi báo lỗi
    client = esp_websocket_client_init(&cfg);
    if (!client)
    {
        ESP_LOGE(TAG, "Failed to init websocket");
        return;
    }

    ESP_ERROR_CHECK(esp_websocket_register_events(
        client,
        WEBSOCKET_EVENT_ANY,
        &WebSocketClient::eventHandlerStatic,
        this));

    ESP_LOGI(TAG, "Free heap before ws_start: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Connecting to WS: %s", ws_url.c_str());
    esp_websocket_client_start(client);
    ESP_LOGI(TAG, "Free heap after ws_start: %d bytes", esp_get_free_heap_size());

    if (!tx_task) {
        run_tx_task = true;
        xTaskCreate(&WebSocketClient::wsTxTaskEntry, "ws_tx", 4096, this, 5, &tx_task);
    }

    if (status_cb)
        status_cb(1); // CONNECTING
}

void WebSocketClient::close()
{
    if (client)
    {
        ESP_LOGI(TAG, "Free heap before close: %d bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "Closing WebSocket...");
        esp_websocket_client_close(client, 100);
        // Give internal task time to cleanup
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_websocket_client_destroy(client);
        client = nullptr;
        connected = false;
        ESP_LOGI(TAG, "Free heap after close: %d bytes", esp_get_free_heap_size());
    }

    run_tx_task = false;
    if (tx_task) {
        vTaskDelay(pdMS_TO_TICKS(100));
        tx_task = nullptr;
    }
    if (tx_buffer) {
        vStreamBufferDelete(tx_buffer);
        tx_buffer = nullptr;
    }

    if (status_cb)
        status_cb(0); // CLOSED
}

bool WebSocketClient::sendText(const std::string &msg)
{
    if (!client || !connected)
        return false;

    int sent = esp_websocket_client_send_text(client, msg.c_str(), msg.size(), 100);
    return sent == msg.size();
}

bool WebSocketClient::sendBinary(const uint8_t *data, size_t len)
{
    if (!client || !connected || !tx_buffer)
        return false;

    // Push into buffer without blocking (wait=0) to keep SPI task perfectly real-time
    size_t sent = xStreamBufferSend(tx_buffer, data, len, 0);
    if (sent < len)
    {
        ESP_LOGW(TAG, "WS TX buffer full, dropping frame part (%zu/%zu sent)", sent, len);
        return false;
    }
    return true;
}

void WebSocketClient::wsTxTaskEntry(void *arg)
{
    static_cast<WebSocketClient *>(arg)->wsTxLoop();
}

void WebSocketClient::wsTxLoop()
{
    ESP_LOGI(TAG, "WS TX Task started");
    uint8_t buf[1024];

    while (run_tx_task) {
        if (!connected || !client || !tx_buffer) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        uint8_t header[2];
        size_t h_got = xStreamBufferReceive(tx_buffer, header, 2, pdMS_TO_TICKS(50));
        if (h_got < 2) continue; // Not enough data yet

        uint16_t frame_len = header[0] | ((uint16_t)header[1] << 8);
        if (frame_len == 0 || frame_len > sizeof(buf) - 2) {
            ESP_LOGE(TAG, "WS TX Task: Invalid frame length %u, flushing buffer", frame_len);
            xStreamBufferReset(tx_buffer);
            continue;
        }

        // Buffer the header so the server receives the prefixed data
        buf[0] = header[0];
        buf[1] = header[1];

        size_t d_got = xStreamBufferReceive(tx_buffer, buf + 2, frame_len, pdMS_TO_TICKS(100));
        if (d_got < frame_len) {
            ESP_LOGE(TAG, "WS TX Task: Incomplete payload (%zu/%u), flushing", d_got, frame_len);
            xStreamBufferReset(tx_buffer);
            continue;
        }

        if (connected) {
            // Send exactly one prefixed Opus frame
            int sent = esp_websocket_client_send_bin(client, (const char *)buf, frame_len + 2, pdMS_TO_TICKS(2000));
            if (sent < 0) {
                ESP_LOGW(TAG, "WS TX Task: send_bin failed");
            }
        }
    }
    ESP_LOGI(TAG, "WS TX Task stopped");
    vTaskDelete(nullptr);
}

void WebSocketClient::onStatus(std::function<void(int)> cb)
{
    status_cb = cb;
}

void WebSocketClient::onText(std::function<void(const std::string &)> cb)
{
    text_cb = cb;
}

void WebSocketClient::onBinary(std::function<void(const uint8_t *, size_t)> cb)
{
    binary_cb = cb;
}

// ======================================================================================
// EVENT HANDLER
// ======================================================================================
void WebSocketClient::eventHandlerStatic(void *handler_args, esp_event_base_t base,
                                         int32_t event_id, void *event_data)
{
    auto *self = static_cast<WebSocketClient *>(handler_args);
    self->eventHandler(base, event_id, (esp_websocket_event_data_t *)event_data);
}

void WebSocketClient::eventHandler(esp_event_base_t base, int32_t event_id,
                                   esp_websocket_event_data_t *data)
{
    switch (event_id)
    {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WS connected!");
        connected = true;
        if (status_cb)
            status_cb(2);
        break;

    case WEBSOCKET_EVENT_DATA:
        if (!data)
            break;

        if (data->op_code == 0x1)
        { // WS_OP_TEXT
            if (text_cb)
            {
                text_cb(std::string((const char *)data->data_ptr, data->data_len));
            }
        }
        else if (data->op_code == 0x2)
        { // WS_OP_BINARY
            if (binary_cb)
            {
                binary_cb((const uint8_t *)data->data_ptr, data->data_len);
            }
        }
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "WS disconnected");
        connected = false;
        if (status_cb)
            status_cb(0);
        break;

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WS error event");
        connected = false;
        if (status_cb)
            status_cb(0);
        break;

    default:
        break;
    }
}
