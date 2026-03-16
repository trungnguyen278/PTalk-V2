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

bool I2SAudioOutput_MAX98357::init()
{
    if (initialized_) return true;

    // Configure TX channel in standard I2S mode
    // Must be init'd early for full-duplex shared clock to work with RX
    i2s_std_config_t std_cfg = {};

    // ESP32-C5 I2S runs ~6.8% faster than configured rate (both PLL and XTAL).
    // Compensate by requesting lower rate so actual output ≈ target.
    uint32_t compensated_rate = cfg_.sample_rate * 1000 / 1068;
    ESP_LOGI(TAG, "I2S clock: target=%luHz, compensated=%luHz",
             (unsigned long)cfg_.sample_rate, (unsigned long)compensated_rate);
    std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(compensated_rate);

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

    // TX channel is kept always-enabled for full-duplex clock (mic needs it).
    // Just mark as running so writePcm works.
    running_ = true;
    ESP_LOGI(TAG, "Playback started");
    return true;
}

void I2SAudioOutput_MAX98357::stopPlayback()
{
    if (!running_) return;

    // Do NOT disable TX channel — it provides the I2S clock for mic (full-duplex).
    // MAX98357 automatically enters shutdown when no data is written.
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
        int32_t scaled = ((int32_t)pcm[i] * volume_) / 100;
        out_buf[i] = scaled << 16;
    }

    // DEBUG: log first few PCM values periodically
    static uint32_t write_count = 0;
    write_count++;
    if (write_count <= 5 || write_count % 500 == 0) {
        // Show min/max in this chunk to detect anomalies
        int16_t mn = pcm[0], mx = pcm[0];
        for (size_t i = 1; i < count; ++i) {
            if (pcm[i] < mn) mn = pcm[i];
            if (pcm[i] > mx) mx = pcm[i];
        }
        ESP_LOGI(TAG, "writePcm[%lu] vol=%d cnt=%zu: min=%d max=%d pcm[0..3]=%d,%d,%d,%d",
                 (unsigned long)write_count, (int)volume_, count,
                 (int)mn, (int)mx,
                 (int)pcm[0], (int)(count>1?pcm[1]:0), (int)(count>2?pcm[2]:0), (int)(count>3?pcm[3]:0));
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
