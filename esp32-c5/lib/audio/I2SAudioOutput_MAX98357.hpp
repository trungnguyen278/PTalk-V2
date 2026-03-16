#pragma once
#include "AudioOutput.hpp"
#include "driver/i2s_std.h"
#include <cstdint>

// MAX98357 I2S Amplifier driver (ESP-IDF 5.x new I2S API)
// Accepts a pre-created TX channel handle for full-duplex sharing with mic.
class I2SAudioOutput_MAX98357 : public AudioOutput {
public:
    struct Config {
        gpio_num_t pin_bclk;    // Shared bit clock
        gpio_num_t pin_ws;      // Shared word select
        gpio_num_t pin_dout;    // Data out to amplifier
        uint32_t   sample_rate = 16000;
    };

    // tx_chan: pre-created I2S TX channel (from i2s_new_channel full-duplex)
    I2SAudioOutput_MAX98357(i2s_chan_handle_t tx_chan, const Config& cfg);
    ~I2SAudioOutput_MAX98357() override;

    bool   init();  // Init TX channel (call early for full-duplex clock)
    bool   startPlayback() override;
    void   stopPlayback() override;
    size_t writePcm(const int16_t* pcm, size_t pcm_samples) override;
    void   setVolume(uint8_t percent) override;
    void   setLowPower(bool enable) override;

    uint32_t sampleRate() const override   { return cfg_.sample_rate; }
    uint8_t  channels() const override     { return 1; }
    uint8_t  bitsPerSample() const override { return 16; }

private:
    i2s_chan_handle_t tx_chan_;
    Config cfg_;
    bool initialized_ = false;
    bool running_     = false;
    uint8_t volume_   = 60;
};
