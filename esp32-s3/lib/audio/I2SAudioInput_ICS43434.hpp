#pragma once

/**
 * File:    I2SAudioInput_ICS43434.hpp
 * Author:  Trung Nguyen
 * GitHub:  https://github.com/trungnguyen278/PTalk-V2
 * Date:    30 Jun 2026
 *
 * Description:
 *  - Part of the PTalk-V2 project
 *  - Written and maintained by Trung Nguyen
 */

#include "AudioInput.hpp"
#include "driver/i2s_std.h"
#include <cstdint>

// ICS43434 I2S MEMS Microphone driver (ESP-IDF 5.x new I2S API)
// Accepts a pre-created RX channel handle for full-duplex sharing with speaker.
class I2SAudioInput_ICS43434 : public AudioInput {
public:
    struct Config {
        gpio_num_t pin_bclk;    // Shared bit clock
        gpio_num_t pin_ws;      // Shared word select
        gpio_num_t pin_din;     // Data in from mic
        uint32_t   sample_rate = 16000;
    };

    // rx_chan: pre-created I2S RX channel (from i2s_new_channel full-duplex)
    I2SAudioInput_ICS43434(i2s_chan_handle_t rx_chan, const Config& cfg);
    ~I2SAudioInput_ICS43434() override;

    bool   init() override;
    bool   startCapture() override;
    void   stopCapture() override;
    void   pauseCapture() override;
    size_t readPcm(int16_t* buffer, size_t max_samples) override;
    void   setMuted(bool muted) override;
    void   setLowPower(bool enable) override;

    uint32_t sampleRate() const override { return cfg_.sample_rate; }
    uint8_t  channels() const override { return 1; }
    uint8_t  bitsPerSample() const override { return 16; }

private:
    i2s_chan_handle_t rx_chan_;
    Config cfg_;
    bool initialized_ = false;
    bool capturing_   = false;
    bool muted_       = false;
};
