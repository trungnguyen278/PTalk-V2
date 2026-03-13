#include "I2SAudioOutput_MAX98357.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <cstring>

static const char* TAG = "MAX98357";

I2SAudioOutput_MAX98357::I2SAudioOutput_MAX98357(i2s_chan_handle_t tx_chan, const Config& cfg)
    : tx_chan_(tx_chan), cfg_(cfg)
{
}

I2SAudioOutput_MAX98357::~I2SAudioOutput_MAX98357()
{
    stopPlayback();
}

bool I2SAudioOutput_MAX98357::startPlayback()
{
    if (running_) return true;

    if (!initialized_) {
        // Configure TX channel in standard I2S mode
        i2s_std_config_t std_cfg = {};

        std_cfg.clk_cfg.sample_rate_hz = cfg_.sample_rate;
        std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
        std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

        // Slot config: 32-bit frames to match mic (full-duplex shared clock)
        std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_32BIT;
        std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
        std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
        std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
        std_cfg.slot_cfg.ws_width = 32;
        std_cfg.slot_cfg.ws_pol = false;
        std_cfg.slot_cfg.bit_shift = true;

        std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
        std_cfg.gpio_cfg.bclk = cfg_.pin_bclk;
        std_cfg.gpio_cfg.ws = cfg_.pin_ws;
        std_cfg.gpio_cfg.dout = cfg_.pin_dout;
        std_cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
        std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
        std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
        std_cfg.gpio_cfg.invert_flags.ws_inv = false;

        esp_err_t err = i2s_channel_init_std_mode(tx_chan_, &std_cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init TX std mode: %s", esp_err_to_name(err));
            return false;
        }
        initialized_ = true;
    }

    esp_err_t err = i2s_channel_enable(tx_chan_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable TX channel: %s", esp_err_to_name(err));
        return false;
    }

    running_ = true;
    ESP_LOGI(TAG, "Playback started");
    return true;
}

void I2SAudioOutput_MAX98357::stopPlayback()
{
    if (!running_) return;

    i2s_channel_disable(tx_chan_);
    running_ = false;
    ESP_LOGI(TAG, "Playback stopped");
}

size_t I2SAudioOutput_MAX98357::writePcm(const int16_t* pcm, size_t pcm_samples)
{
    if (!running_ || volume_ == 0 || !pcm || pcm_samples == 0)
        return 0;

    // Convert 16-bit PCM to 32-bit (left-aligned) with volume scaling
    static int32_t out_buf[1024];
    size_t count = (pcm_samples > 1024) ? 1024 : pcm_samples;

    for (size_t i = 0; i < count; ++i) {
        int16_t scaled = (int16_t)((pcm[i] * volume_) / 100);
        out_buf[i] = ((int32_t)scaled) << 16;
    }

    size_t bytes_written = 0;
    esp_err_t err = i2s_channel_write(tx_chan_, out_buf,
                                       count * sizeof(int32_t),
                                       &bytes_written, pdMS_TO_TICKS(50));
    if (err != ESP_OK) return 0;

    return bytes_written / sizeof(int32_t);
}

void I2SAudioOutput_MAX98357::setVolume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    volume_ = percent;
}

void I2SAudioOutput_MAX98357::setLowPower(bool enable)
{
    if (enable) stopPlayback();
}
