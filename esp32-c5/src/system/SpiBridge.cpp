#include "SpiBridge.hpp"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "SpiBridge";

SpiBridge::~SpiBridge()
{
    stop();
    if (ctrl_queue_)  { vQueueDelete(ctrl_queue_); ctrl_queue_ = nullptr; }
    if (dl_audio_sb_) { vStreamBufferDelete(dl_audio_sb_); dl_audio_sb_ = nullptr; }
}

bool SpiBridge::init(const Config& cfg)
{
    cfg_ = cfg;

    // Init handshake GPIO as output (LOW = no data)
    gpio_config_t hs_cfg = {};
    hs_cfg.pin_bit_mask = (1ULL << cfg_.pin_handshake);
    hs_cfg.mode = GPIO_MODE_OUTPUT;
    hs_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    hs_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&hs_cfg);
    gpio_set_level(cfg_.pin_handshake, 0);

    // Init SPI slave
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = cfg_.pin_mosi;
    bus_cfg.miso_io_num = cfg_.pin_miso;
    bus_cfg.sclk_io_num = cfg_.pin_sclk;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = spi_proto::FRAME_SIZE;

    spi_slave_interface_config_t slave_cfg = {};
    slave_cfg.mode = 0;
    slave_cfg.spics_io_num = cfg_.pin_cs;
    slave_cfg.queue_size = 1;
    slave_cfg.flags = 0;

    esp_err_t err = spi_slave_initialize(SPI2_HOST, &bus_cfg, &slave_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI slave init failed: %s", esp_err_to_name(err));
        return false;
    }

    // Audio downlink stream buffer: 32KB — stores raw [len][opus] frames
    // At ~150 bytes/frame, holds ~200 frames (~4 seconds of audio)
    // C5 no longer runs audio/Opus/I2S, so heap is plentiful
    dl_audio_sb_ = xStreamBufferCreate(32 * 1024, 1);
    if (!dl_audio_sb_) {
        ESP_LOGE(TAG, "DL audio stream buffer create failed");
        return false;
    }

    // Control/status queue: small, only for STATUS_UPDATE messages
    ctrl_queue_ = xQueueCreate(4, spi_proto::FRAME_SIZE);
    if (!ctrl_queue_) {
        ESP_LOGE(TAG, "Ctrl queue create failed");
        return false;
    }

    ESP_LOGI(TAG, "SpiBridge init OK (MOSI=%d MISO=%d SCLK=%d CS=%d HS=%d)",
             cfg_.pin_mosi, cfg_.pin_miso, cfg_.pin_sclk, cfg_.pin_cs,
             cfg_.pin_handshake);
    return true;
}

void SpiBridge::start()
{
    if (started_) return;
    started_ = true;

    xTaskCreatePinnedToCore(&SpiBridge::slaveTaskEntry, "SpiBridge", 4096, this, 5, &slave_task_, 0);
    ESP_LOGI(TAG, "SpiBridge started");
}

void SpiBridge::stop()
{
    if (!started_) return;
    started_ = false;
    vTaskDelay(pdMS_TO_TICKS(200));
    slave_task_ = nullptr;
    ESP_LOGI(TAG, "SpiBridge stopped");
}

bool SpiBridge::sendAudioDownlink(const uint8_t* data, size_t len)
{
    if (!dl_audio_sb_ || len == 0 || len > spi_proto::MAX_PAYLOAD) return false;

    // Push raw [len][opus] frame into stream buffer — non-blocking
    size_t sent = xStreamBufferSend(dl_audio_sb_, data, len, 0);
    if (sent < len) {
        ESP_LOGW(TAG, "DL audio buffer full, frame dropped (%zu/%zu)", sent, len);
        return false;
    }
    // Signal master that we have data
    gpio_set_level(cfg_.pin_handshake, 1);
    return true;
}

bool SpiBridge::sendStatusUpdate(uint8_t interaction, uint8_t connectivity,
                                  uint8_t system_state, uint8_t emotion)
{
    if (!ctrl_queue_) return false;

    spi_proto::StatusPayload sp;
    sp.interaction = interaction;
    sp.connectivity = connectivity;
    sp.system_state = system_state;
    sp.emotion = emotion;

    uint8_t frame[spi_proto::FRAME_SIZE];
    spi_proto::buildFrame(frame, (uint8_t)spi_proto::MsgFromC5::STATUS_UPDATE,
                          (const uint8_t*)&sp, sizeof(sp), tx_seq_++);

    bool ok = xQueueSend(ctrl_queue_, frame, 0) == pdTRUE;
    updateHandshake();
    return ok;
}

void SpiBridge::updateHandshake()
{
    bool has_data = (dl_audio_sb_ && xStreamBufferBytesAvailable(dl_audio_sb_) > 0) ||
                    (ctrl_queue_ && uxQueueMessagesWaiting(ctrl_queue_) > 0);
    gpio_set_level(cfg_.pin_handshake, has_data ? 1 : 0);
}

void SpiBridge::handleRxFrame(const uint8_t* rx_buf)
{
    uint8_t msg_type;
    const uint8_t* payload;
    uint16_t payload_len;
    uint8_t seq;

    if (!spi_proto::parseFrame(rx_buf, msg_type, payload, payload_len, seq)) {
        return;
    }

    if (msg_type == (uint8_t)spi_proto::MsgFromS3::AUDIO_UPLINK) {
        if (audio_ul_cb_ && payload_len > 0) {
            audio_ul_cb_(payload, payload_len);
        }
    } else if (msg_type == (uint8_t)spi_proto::MsgFromS3::CONTROL_CMD) {
        if (control_cb_ && payload_len >= 1) {
            auto cmd = static_cast<spi_proto::ControlCmd>(payload[0]);
            control_cb_(cmd, &payload[1], payload_len - 1);
        }
    }
    // HEARTBEAT: no action needed (just polls for C5 response)
}

// === Prepare next TX frame ===
// Priority: control/status > audio downlink > EMPTY
bool SpiBridge::prepareTxFrame(uint8_t* tx_buf)
{
    // 1. Control/status messages first (rare but important)
    if (ctrl_queue_ && uxQueueMessagesWaiting(ctrl_queue_) > 0) {
        xQueueReceive(ctrl_queue_, tx_buf, 0);
        return true;
    }

    // 2. Audio downlink: read one complete [2B len][opus] frame from stream buffer
    if (dl_audio_sb_ && xStreamBufferBytesAvailable(dl_audio_sb_) >= 3) {
        uint8_t header[2];
        size_t got = xStreamBufferReceive(dl_audio_sb_, header, 2, 0);
        if (got == 2) {
            uint16_t frame_len = header[0] | ((uint16_t)header[1] << 8);
            if (frame_len > 0 && frame_len <= 500 && (size_t)(2 + frame_len) <= spi_proto::MAX_PAYLOAD) {
                // Build payload: [2B len][opus data]
                uint8_t payload[spi_proto::MAX_PAYLOAD];
                payload[0] = header[0];
                payload[1] = header[1];
                size_t data_got = xStreamBufferReceive(dl_audio_sb_, &payload[2], frame_len, 0);
                if (data_got == frame_len) {
                    spi_proto::buildFrame(tx_buf, (uint8_t)spi_proto::MsgFromC5::AUDIO_DOWNLINK,
                                          payload, (uint16_t)(2 + frame_len), tx_seq_++);
                    return true;
                } else {
                    // Incomplete frame in stream buffer — discard and reset
                    ESP_LOGW(TAG, "DL audio: incomplete frame (%zu/%u), flushing", data_got, frame_len);
                    xStreamBufferReset(dl_audio_sb_);
                }
            } else {
                // Bad frame length — stream is corrupted, reset
                ESP_LOGW(TAG, "DL audio: bad frame len %u, flushing", frame_len);
                xStreamBufferReset(dl_audio_sb_);
            }
        }
    }

    // 3. Nothing to send
    spi_proto::buildEmptyFrame(tx_buf, tx_seq_++);
    return false;
}

// === Slave Task ===

void SpiBridge::slaveTaskEntry(void* arg)
{
    static_cast<SpiBridge*>(arg)->slaveLoop();
}

void SpiBridge::slaveLoop()
{
    ESP_LOGI(TAG, "Slave task started");

    // DMA-capable buffers
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
        // Prepare TX frame from stream buffer / ctrl queue
        prepareTxFrame(tx_buf);

        // Update handshake: HIGH if more data waiting after this frame
        updateHandshake();

        // Queue slave transaction and wait for master
        spi_slave_transaction_t t = {};
        t.length = spi_proto::FRAME_SIZE * 8;
        t.tx_buffer = tx_buf;
        t.rx_buffer = rx_buf;

        esp_err_t err = spi_slave_transmit(SPI2_HOST, &t, pdMS_TO_TICKS(100));
        if (err == ESP_OK) {
            handleRxFrame(rx_buf);
        }
        // ESP_ERR_TIMEOUT is normal when master hasn't initiated a transaction
    }

    free(tx_buf);
    free(rx_buf);
    ESP_LOGW(TAG, "Slave task ended");
    vTaskDelete(nullptr);
}
