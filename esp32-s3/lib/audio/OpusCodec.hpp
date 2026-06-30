#pragma once

/**
 * File:    OpusCodec.hpp
 * Author:  Trung Nguyen
 * GitHub:  https://github.com/trungnguyen278/PTalk-V2
 * Date:    30 Jun 2026
 *
 * Description:
 *  - Part of the PTalk-V2 project
 *  - Written and maintained by Trung Nguyen
 */

#include "AudioCodec.hpp"
#include <cstdint>
#include <mutex>

// Forward declare opus types to avoid pulling in opus.h in the header
struct OpusEncoder;
struct OpusDecoder;

// Opus codec implementation for PTalk V2
// Replaces ADPCM with much better audio quality at similar bitrates.
// Frame size: 320 samples (20ms @ 16kHz), variable-length output.
//
// Encoding: PCM frames -> Opus frames (prefixed with 2-byte LE length)
// Decoding: Opus frames (length-prefixed) -> PCM frames
class OpusCodec : public AudioCodec {
public:
    OpusCodec();
    ~OpusCodec() override;

    // AudioCodec interface
    size_t encode(const int16_t* pcm_in, size_t pcm_samples,
                  uint8_t* encoded_out, size_t encoded_capacity) override;

    size_t decode(const uint8_t* encoded_in, size_t encoded_bytes,
                  int16_t* pcm_out, size_t pcm_capacity) override;

    void reset() override;

    size_t   pcmFrameSamples()   const override { return FRAME_SAMPLES; }
    size_t   encodedFrameBytes() const override { return MAX_ENCODED_BYTES; }
    uint32_t sampleRate()        const override { return 48000; }
    uint8_t  channels()          const override { return 1; }

    // Configuration
    void setBitrate(int bitrate_bps);

private:
    static constexpr size_t FRAME_SAMPLES      = 960;   // 20ms @ 48kHz
    static constexpr size_t MAX_ENCODED_BYTES   = 512;   // Max Opus frame at 48kHz
    static constexpr int    DEFAULT_BITRATE     = 64000; // 64 kbps

    OpusEncoder* encoder_ = nullptr;
    OpusDecoder* decoder_ = nullptr;
    bool initialized_ = false;
    std::mutex mutex_;

    bool initCodec();
};
