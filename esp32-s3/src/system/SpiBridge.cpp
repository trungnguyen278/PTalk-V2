#include "SpiBridge.hpp"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "SpiBridge";

SpiBridge::~SpiBridge()
{
    stop();
    if (tx_queue_) { vQueueDelete(tx_queue_); tx_queue_ = nullptr; }
}

bool SpiBridge::init(const Config& cfg)
{
    cfg_ = cfg;

    // Init handshake GPIO as input
    gpio_config_t hs_cfg = {};
    hs_cfg.pin_bit_mask = (1ULL << cfg_.pin_handshake);
    hs_cfg.mode = GPIO_MODE_INPUT;
    hs_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    hs_cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
    gpio_config(&hs_cfg);

    // Init SPI3 bus
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = cfg_.pin_mosi;
    bus_cfg.miso_io_num = cfg_.pin_miso;
    bus_cfg.sclk_io_num = cfg_.pin_sclk;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = spi_proto::FRAME_SIZE;

    esp_err_t err = spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(err));
        return false;
    }

    // Add SPI device
    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.clock_speed_hz = cfg_.clock_speed_hz;
    dev_cfg.mode = 0;
    dev_cfg.spics_io_num = cfg_.pin_cs;
    dev_cfg.queue_size = 1;
    dev_cfg.flags = 0;

    err = spi_bus_add_device(SPI3_HOST, &dev_cfg, &spi_dev_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(err));
        return false;
    }

    // TX queue: holds 256-byte frames (16 deep for audio burst tolerance)
    tx_queue_ = xQueueCreate(16, spi_proto::FRAME_SIZE);
    if (!tx_queue_) {
        ESP_LOGE(TAG, "TX queue create failed");
        return false;
    }

    ESP_LOGI(TAG, "SpiBridge init OK (MOSI=%d MISO=%d SCLK=%d CS=%d HS=%d %dMHz)",
             cfg_.pin_mosi, cfg_.pin_miso, cfg_.pin_sclk, cfg_.pin_cs,
             cfg_.pin_handshake, cfg_.clock_speed_hz / 1000000);
    return true;
}

void SpiBridge::start()
{
    if (started_) return;
    started_ = true;

    xTaskCreatePinnedToCore(&SpiBridge::pollTaskEntry, "SpiBridge", 4096, this, 3, &poll_task_, 1);
    ESP_LOGI(TAG, "SpiBridge started");
}

void SpiBridge::stop()
{
    if (!started_) return;
    started_ = false;
    vTaskDelay(pdMS_TO_TICKS(200));
    poll_task_ = nullptr;
    ESP_LOGI(TAG, "SpiBridge stopped");
}

bool SpiBridge::sendAudioUplink(const uint8_t* data, size_t len)
{
    if (!tx_queue_ || len == 0 || len > spi_proto::MAX_PAYLOAD) return false;

    uint8_t frame[spi_proto::FRAME_SIZE];
    spi_proto::buildFrame(frame, (uint8_t)spi_proto::MsgFromS3::AUDIO_UPLINK,
                          data, (uint16_t)len, tx_seq_++);

    // Wait up to 10ms — gives poll loop time to drain one frame
    return xQueueSend(tx_queue_, frame, pdMS_TO_TICKS(10)) == pdTRUE;
}

bool SpiBridge::sendControlCmd(spi_proto::ControlCmd cmd, const uint8_t* data, size_t len)
{
    if (!tx_queue_) return false;

    uint8_t payload[spi_proto::MAX_PAYLOAD];
    payload[0] = (uint8_t)cmd;
    if (data && len > 0 && len < spi_proto::MAX_PAYLOAD) {
        memcpy(&payload[1], data, len);
    }

    uint8_t frame[spi_proto::FRAME_SIZE];
    spi_proto::buildFrame(frame, (uint8_t)spi_proto::MsgFromS3::CONTROL_CMD,
                          payload, (uint16_t)(1 + len), tx_seq_++);

    return xQueueSend(tx_queue_, frame, 0) == pdTRUE;
}

// === SPI Transaction ===

bool SpiBridge::doTransaction(const uint8_t* tx_buf, uint8_t* rx_buf)
{
    spi_transaction_t t = {};
    t.length = spi_proto::FRAME_SIZE * 8;  // bits
    t.tx_buffer = tx_buf;
    t.rx_buffer = rx_buf;

    esp_err_t err = spi_device_transmit(spi_dev_, &t);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI transaction failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

void SpiBridge::handleRxFrame(const uint8_t* rx_buf)
{
    uint8_t msg_type;
    const uint8_t* payload;
    uint16_t payload_len;
    uint8_t seq;

    if (!spi_proto::parseFrame(rx_buf, msg_type, payload, payload_len, seq)) {
        return; // Invalid frame or EMPTY
    } else if (msg_type == (uint8_t)spi_proto::MsgFromC5::EMPTY) {
        // ESP_LOGD(TAG, "RX EMPTY");
        return;
    }

    if (msg_type == (uint8_t)spi_proto::MsgFromC5::AUDIO_DOWNLINK) {
        if (audio_dl_cb_ && payload_len > 0) {
            audio_dl_cb_(payload, payload_len);
        }
    } else if (msg_type == (uint8_t)spi_proto::MsgFromC5::STATUS_UPDATE) {
        if (status_cb_ && payload_len >= sizeof(spi_proto::StatusPayload)) {
            auto* sp = reinterpret_cast<const spi_proto::StatusPayload*>(payload);
            ESP_LOGI(TAG, "RX STATUS_UPDATE: int=%d, conn=%d, sys=%d, emo=%d", 
                     sp->interaction, sp->connectivity, sp->system_state, sp->emotion);
            status_cb_(sp->interaction, sp->connectivity, sp->system_state, sp->emotion);
        } else {
            ESP_LOGW(TAG, "RX STATUS_UPDATE failed: status_cb_=%d, payload_len=%d (expected %d)", 
                     status_cb_ != nullptr, payload_len, sizeof(spi_proto::StatusPayload));
        }
    }
}

// === Poll Task ===

void SpiBridge::pollTaskEntry(void* arg)
{
    static_cast<SpiBridge*>(arg)->pollLoop();
}

void SpiBridge::pollLoop()
{
    ESP_LOGI(TAG, "Poll task started");

    // DMA-capable buffers (must be in internal RAM for SPI DMA)
    uint8_t* tx_buf = (uint8_t*)heap_caps_malloc(spi_proto::FRAME_SIZE, MALLOC_CAP_DMA);
    uint8_t* rx_buf = (uint8_t*)heap_caps_malloc(spi_proto::FRAME_SIZE, MALLOC_CAP_DMA);

    if (!tx_buf || !rx_buf) {
        ESP_LOGE(TAG, "Failed to allocate DMA buffers");
        free(tx_buf);
        free(rx_buf);
        vTaskDelete(nullptr);
        return;
    }

    while (started_) {
        bool has_tx = (uxQueueMessagesWaiting(tx_queue_) > 0);
        bool c5_ready = (gpio_get_level(cfg_.pin_handshake) == 1);

        if (!has_tx && !c5_ready) {
            // Nothing to do — yield to let IDLE task run
            TickType_t delay_ticks = pdMS_TO_TICKS(5);
            vTaskDelay(delay_ticks > 0 ? delay_ticks : 1);
            continue;
        }

        static bool last_c5_ready = false;
        if (c5_ready != last_c5_ready) {
            ESP_LOGI(TAG, "c5_ready changed to %d", c5_ready);
            last_c5_ready = c5_ready;
        }

        // Prepare TX frame
        if (has_tx) {
            xQueueReceive(tx_queue_, tx_buf, 0);
        } else {
            // Send heartbeat to poll C5
            spi_proto::buildFrame(tx_buf, (uint8_t)spi_proto::MsgFromS3::HEARTBEAT,
                                  nullptr, 0, tx_seq_++);
        }

        // Full-duplex transaction
        if (doTransaction(tx_buf, rx_buf)) {
            handleRxFrame(rx_buf);
        }

        // Yield after every transaction so IDLE1 can reset the watchdog
        TickType_t yield_ticks = pdMS_TO_TICKS(1);
        vTaskDelay(yield_ticks > 0 ? yield_ticks : 1);
    }

    free(tx_buf);
    free(rx_buf);
    ESP_LOGW(TAG, "Poll task ended");
    vTaskDelete(nullptr);
}
