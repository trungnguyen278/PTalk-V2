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

void I2SAudioOutput_MAX98357::resetFilter()
{
    bq1_ = {};
    bq2_ = {};
}

bool I2SAudioOutput_MAX98357::init()
{
    if (initialized_) return true;

    i2s_std_config_t std_cfg = {};

    // Use configured rate directly — compensation may not be needed at 48kHz
    ESP_LOGI(TAG, "I2S clock: rate=%luHz", (unsigned long)cfg_.sample_rate);
    std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(cfg_.sample_rate);

    // Philips MONO left-only for MAX98357 (SD=HIGH → left channel)
    std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO);
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

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
    ESP_LOGI(TAG, "MAX98357 TX channel initialized (MONO 32-bit)");
    return true;
}

bool I2SAudioOutput_MAX98357::startPlayback()
{
    if (running_) return true;
    if (!initialized_ && !init()) return false;

    prev_sample_ = 0;
    running_ = true;
    ESP_LOGI(TAG, "Playback started");
    return true;
}

void I2SAudioOutput_MAX98357::stopPlayback()
{
    if (!running_) return;
    running_ = false;
    ESP_LOGI(TAG, "Playback stopped");
}

size_t I2SAudioOutput_MAX98357::writePcm(const int16_t* pcm, size_t pcm_samples)
{
    if (!running_ || volume_ == 0 || !pcm || pcm_samples == 0)
        return 0;

    // Convert 16-bit PCM to 32-bit (left-aligned) with volume scaling
    static int32_t out_buf[512];
    size_t count = (pcm_samples > 512) ? 512 : pcm_samples;

    for (size_t i = 0; i < count; ++i) {
        // Use fixed-point multiply with rounding to reduce quantization noise.
        // vol_scale = volume * 327 ≈ volume * 32768/100 (Q15 fraction)
        int32_t scaled = ((int32_t)pcm[i] * (int32_t)(volume_ * 327)) >> 15;
        out_buf[i] = scaled << 16;
    }

    // DEBUG: periodic status
    static uint32_t write_count = 0;
    write_count++;
    if (write_count <= 3 || write_count % 2000 == 0) {
        int16_t mn = pcm[0], mx = pcm[0];
        for (size_t i = 1; i < count; ++i) {
            if (pcm[i] < mn) mn = pcm[i];
            if (pcm[i] > mx) mx = pcm[i];
        }
        ESP_LOGI(TAG, "writePcm[%lu] vol=%d cnt=%zu: min=%d max=%d",
                 (unsigned long)write_count, (int)volume_, count,
                 (int)mn, (int)mx);
    }

    size_t bytes_written = 0;
    esp_err_t err = i2s_channel_write(tx_chan_, out_buf,
                                       count * sizeof(int32_t),
                                       &bytes_written, pdMS_TO_TICKS(200));

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
