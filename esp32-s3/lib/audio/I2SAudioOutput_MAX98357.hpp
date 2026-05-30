#pragma once
#include "AudioOutput.hpp"
#include "driver/i2s_std.h"
#include <cstdint>

// MAX98357 I2S Amplifier driver (ESP-IDF 5.x new I2S API)
// Accepts a pre-created TX channel handle for full-duplex sharing with mic.
// I2S runs at 2x the audio sample rate (e.g., 32kHz for 16kHz audio).
// writePcm performs 2x linear interpolation to smooth the output for
// the filterless Class-D amplifier.
class I2SAudioOutput_MAX98357 : public AudioOutput {
public:
    struct Config {
        gpio_num_t pin_bclk;    // Shared bit clock
        gpio_num_t pin_ws;      // Shared word select
        gpio_num_t pin_dout;    // Data out to amplifier
        gpio_num_t pin_sd = GPIO_NUM_NC;  // Shutdown pin (LOW=mute, HIGH=active)
        uint32_t   sample_rate = 16000;
    };

    // Biquad filter state (reserved for future use)
    struct BiquadState { int32_t x1=0, x2=0, y1=0, y2=0; };

    // tx_chan: pre-created I2S TX channel (from i2s_new_channel full-duplex)
    I2SAudioOutput_MAX98357(i2s_chan_handle_t tx_chan, const Config& cfg);
    ~I2SAudioOutput_MAX98357() override;

    bool   init();  // Init TX channel (call early for full-duplex clock)
    bool   startPlayback() override;
    void   stopPlayback() override;
    void   flushSilence() override;
    void   enableTxClock() override;
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

    // 2x oversampling state
    int32_t prev_sample_ = 0;  // last sample for inter-call interpolation

    // Biquad LPF (reserved)
    BiquadState bq1_, bq2_;
    void resetFilter();
    void ensureTxEnabled();
};
