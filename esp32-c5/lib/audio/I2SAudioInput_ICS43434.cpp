#include "I2SAudioInput_ICS43434.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <cstring>

static const char* TAG = "ICS43434";

I2SAudioInput_ICS43434::I2SAudioInput_ICS43434(i2s_chan_handle_t rx_chan, const Config& cfg)
    : rx_chan_(rx_chan), cfg_(cfg)
{
}

I2SAudioInput_ICS43434::~I2SAudioInput_ICS43434()
{
    stopCapture();
}

bool I2SAudioInput_ICS43434::init()
{
    if (initialized_) return true;

    // Configure RX channel in standard I2S (Philips) mode
    i2s_std_config_t std_cfg = {};

    // Clock config
    std_cfg.clk_cfg.sample_rate_hz = cfg_.sample_rate;
    std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    // Slot config: ICS43434 outputs 24-bit data in 32-bit frames
    std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_32BIT;
    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
    std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT; // ICS43434 L/R pin determines channel
    std_cfg.slot_cfg.ws_width = 32;
    std_cfg.slot_cfg.ws_pol = false;
    std_cfg.slot_cfg.bit_shift = true; // I2S Philips standard

    // GPIO config
    std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.bclk = cfg_.pin_bclk;
    std_cfg.gpio_cfg.ws = cfg_.pin_ws;
    std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.din = cfg_.pin_din;
    std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.ws_inv = false;

    esp_err_t err = i2s_channel_init_std_mode(rx_chan_, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init RX std mode: %s", esp_err_to_name(err));
        return false;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "ICS43434 initialized (SR=%lu Hz)", (unsigned long)cfg_.sample_rate);
    return true;
}

bool I2SAudioInput_ICS43434::startCapture()
{
    if (!initialized_ || capturing_) return capturing_;

    esp_err_t err = i2s_channel_enable(rx_chan_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RX channel: %s", esp_err_to_name(err));
        return false;
    }

    capturing_ = true;
    ESP_LOGI(TAG, "Capture started");
    return true;
}

void I2SAudioInput_ICS43434::stopCapture()
{
    if (!capturing_) return;

    esp_err_t err = i2s_channel_disable(rx_chan_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to disable RX channel: %s", esp_err_to_name(err));
    }

    capturing_ = false;
    ESP_LOGI(TAG, "Capture stopped");
}

void I2SAudioInput_ICS43434::pauseCapture()
{
    stopCapture();
}

size_t I2SAudioInput_ICS43434::readPcm(int16_t* buffer, size_t max_samples)
{
    if (!capturing_ || muted_ || !buffer || max_samples == 0) return 0;

    // Read 32-bit samples from I2S
    const size_t read_bytes = max_samples * sizeof(int32_t);
    int32_t raw_buf[256]; // Max 256 samples per read
    size_t actual_samples = (max_samples > 256) ? 256 : max_samples;
    size_t bytes_read = 0;

    esp_err_t err = i2s_channel_read(rx_chan_, raw_buf,
                                      actual_samples * sizeof(int32_t),
                                      &bytes_read, pdMS_TO_TICKS(100));
    if (err != ESP_OK || bytes_read == 0) return 0;

    size_t samples = bytes_read / sizeof(int32_t);

    // Convert 32-bit to 16-bit: ICS43434 outputs 24-bit left-aligned in 32-bit
    for (size_t i = 0; i < samples; ++i) {
        buffer[i] = (int16_t)(raw_buf[i] >> 16);
    }

    return samples;
}

void I2SAudioInput_ICS43434::setMuted(bool muted)
{
    muted_ = muted;
}

void I2SAudioInput_ICS43434::setLowPower(bool enable)
{
    if (enable) stopCapture();
}
