/**
 * File:    I2SAudioOutput_MAX98357.cpp
 * Author:  Trung Nguyen
 * GitHub:  https://github.com/trungnguyen278/PTalk-V2
 * Date:    30 Jun 2026
 *
 * Description:
 *  - Part of the PTalk-V2 project
 *  - Written and maintained by Trung Nguyen
 */

#include "I2SAudioOutput_MAX98357.hpp"
#include "esp_log.h"
#include "driver/gpio.h"
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

    esp_err_t dis_err = i2s_channel_disable(tx_chan_);
    esp_err_t en_err  = i2s_channel_enable(tx_chan_);
    ESP_LOGI(TAG, "startPlayback DMA reset: disable=%s enable=%s",
             esp_err_to_name(dis_err), esp_err_to_name(en_err));

    if (cfg_.pin_sd != GPIO_NUM_NC) {
        gpio_set_level(cfg_.pin_sd, 1);
    }

    ESP_LOGI(TAG, "Playback started (SD=HIGH)");
    return true;
}

void I2SAudioOutput_MAX98357::stopPlayback()
{
    if (!running_) return;
    running_ = false;

    if (cfg_.pin_sd != GPIO_NUM_NC) {
        gpio_set_level(cfg_.pin_sd, 0);
    }

    esp_err_t dis_err = i2s_channel_disable(tx_chan_);

    // Preload silence so next enable (playback or mic clock) starts clean.
    static int32_t silence[480] = {};
    size_t total_loaded = 0;
    for (int i = 0; i < 6; i++) {
        size_t loaded = 0;
        esp_err_t err = i2s_channel_preload_data(tx_chan_, silence,
                                                  sizeof(silence), &loaded);
        if (err != ESP_OK || loaded == 0) break;
        total_loaded += loaded;
    }

    // Leave TX disabled — no clock to amp means no noise.
    // enableTxClock() will re-enable when mic needs BCLK/WS.
    ESP_LOGI(TAG, "stopPlayback: disable=%s preload=%zuB (TX stays off)",
             esp_err_to_name(dis_err), total_loaded);
}

void I2SAudioOutput_MAX98357::enableTxClock()
{
    esp_err_t err = i2s_channel_enable(tx_chan_);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "TX enabled for mic clock");
    }
}

size_t I2SAudioOutput_MAX98357::writePcm(const int16_t* pcm, size_t pcm_samples)
{
    if (!running_ || volume_ == 0 || !pcm || pcm_samples == 0)
        return 0;

    // Convert 16-bit PCM to 32-bit (left-aligned) with volume scaling
    static int32_t out_buf[512];
    size_t count = (pcm_samples > 512) ? 512 : pcm_samples;

    for (size_t i = 0; i < count; ++i) {
        int32_t scaled = ((int32_t)pcm[i] * (int32_t)(volume_ * 327)) >> 15;
        out_buf[i] = scaled << 16;
    }

    size_t bytes_written = 0;
    esp_err_t err = i2s_channel_write(tx_chan_, out_buf,
                                       count * sizeof(int32_t),
                                       &bytes_written, pdMS_TO_TICKS(200));
    if (err == ESP_ERR_INVALID_STATE) {
        if (!running_) {
            ESP_LOGW(TAG, "writePcm: channel disabled by stopPlayback, dropping %zu samples", count);
            return 0;
        }
        ensureTxEnabled();
        err = i2s_channel_write(tx_chan_, out_buf,
                                 count * sizeof(int32_t),
                                 &bytes_written, pdMS_TO_TICKS(200));
    }

    if (err != ESP_OK) return 0;
    return bytes_written / sizeof(int32_t);
}

void I2SAudioOutput_MAX98357::ensureTxEnabled()
{
    // On ESP32-S3 full-duplex, disabling the RX channel can knock
    // the TX channel out of RUNNING state.  Re-enable it if needed.
    esp_err_t err = i2s_channel_enable(tx_chan_);
    if (err == ESP_OK) {
        ESP_LOGW(TAG, "TX channel was not running, re-enabled");
    }
}

void I2SAudioOutput_MAX98357::flushSilence()
{
    static int32_t silence[480] = {};
    for (int i = 0; i < 6; i++) {
        size_t written = 0;
        esp_err_t err = i2s_channel_write(tx_chan_, silence, sizeof(silence),
                                           &written, pdMS_TO_TICKS(15));
        if (err == ESP_ERR_INVALID_STATE) {
            ensureTxEnabled();
            err = i2s_channel_write(tx_chan_, silence, sizeof(silence),
                                     &written, pdMS_TO_TICKS(15));
        }
        if (err != ESP_OK || written == 0) break;
    }
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
