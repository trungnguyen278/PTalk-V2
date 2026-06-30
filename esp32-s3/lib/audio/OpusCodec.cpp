/**
 * File:    OpusCodec.cpp
 * Author:  Trung Nguyen
 * GitHub:  https://github.com/trungnguyen278/PTalk-V2
 * Date:    30 Jun 2026
 *
 * Description:
 *  - Part of the PTalk-V2 project
 *  - Written and maintained by Trung Nguyen
 */

#include "OpusCodec.hpp"
#include "esp_log.h"
#include <cstring>

// Opus library header
#include "opus.h"

static const char* TAG = "OpusCodec";

OpusCodec::OpusCodec()
{
    initCodec();
}

OpusCodec::~OpusCodec()
{
    if (encoder_) {
        opus_encoder_destroy(encoder_);
        encoder_ = nullptr;
    }
    if (decoder_) {
        opus_decoder_destroy(decoder_);
        decoder_ = nullptr;
    }
}

bool OpusCodec::initCodec()
{
    if (initialized_) return true;

    int err;

    // Create encoder (48kHz for CELT mode — cleaner frame boundaries)
    encoder_ = opus_encoder_create(48000, 1, OPUS_APPLICATION_AUDIO, &err);
    if (err != OPUS_OK || !encoder_) {
        ESP_LOGE(TAG, "Failed to create Opus encoder: %s", opus_strerror(err));
        return false;
    }

    // Configure encoder — S3 has 240MHz Xtensa dual-core, plenty of CPU on Core 1
    opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(DEFAULT_BITRATE));
    opus_encoder_ctl(encoder_, OPUS_SET_COMPLEXITY(3));          // Balanced: better than 1, won't starve IDLE1 watchdog
    opus_encoder_ctl(encoder_, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(encoder_, OPUS_SET_DTX(0));                 // Disable DTX for real-time
    opus_encoder_ctl(encoder_, OPUS_SET_INBAND_FEC(0));
    opus_encoder_ctl(encoder_, OPUS_SET_VBR(1));                 // Variable bitrate (better quality)

    // Create decoder
    decoder_ = opus_decoder_create(48000, 1, &err);
    if (err != OPUS_OK || !decoder_) {
        ESP_LOGE(TAG, "Failed to create Opus decoder: %s", opus_strerror(err));
        opus_encoder_destroy(encoder_);
        encoder_ = nullptr;
        return false;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Opus codec initialized (48kHz mono, %d bps, 20ms frames)", DEFAULT_BITRATE);
    return true;
}

// Encode PCM to Opus.
// Output format: [opus_frame_len:2 LE] [opus_data:N]
// This length-prefix allows the decoder to know frame boundaries.
size_t OpusCodec::encode(const int16_t* pcm_in, size_t pcm_samples,
                         uint8_t* encoded_out, size_t encoded_capacity)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !pcm_in || !encoded_out) return 0;
    if (pcm_samples < FRAME_SAMPLES) return 0;
    if (encoded_capacity < MAX_ENCODED_BYTES + 2) return 0;

    // Encode one frame (20ms = 320 samples)
    int encoded_len = opus_encode(encoder_, pcm_in, FRAME_SAMPLES,
                                   encoded_out + 2, encoded_capacity - 2);
    if (encoded_len < 0) {
        ESP_LOGE(TAG, "Opus encode error: %s", opus_strerror(encoded_len));
        return 0;
    }

    // Prefix with 2-byte little-endian length
    encoded_out[0] = (uint8_t)(encoded_len & 0xFF);
    encoded_out[1] = (uint8_t)((encoded_len >> 8) & 0xFF);

    return (size_t)(encoded_len + 2);
}

// Decode Opus to PCM.
// Input: raw Opus data (no length prefix — header already parsed by caller)
size_t OpusCodec::decode(const uint8_t* encoded_in, size_t encoded_bytes,
                         int16_t* pcm_out, size_t pcm_capacity)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !encoded_in || !pcm_out) return 0;
    if (encoded_bytes == 0) return 0;
    if (pcm_capacity < FRAME_SAMPLES) return 0;

    int decoded_samples = opus_decode(decoder_, encoded_in, encoded_bytes,
                                       pcm_out, FRAME_SAMPLES, 0);
    if (decoded_samples < 0) {
        ESP_LOGE(TAG, "Opus decode error: %s", opus_strerror(decoded_samples));
        return 0;
    }

    return (size_t)decoded_samples;
}

void OpusCodec::reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (encoder_) {
        opus_encoder_ctl(encoder_, OPUS_RESET_STATE);
    }
    if (decoder_) {
        opus_decoder_ctl(decoder_, OPUS_RESET_STATE);
    }
    ESP_LOGD(TAG, "Codec state reset");
}

void OpusCodec::setBitrate(int bitrate_bps)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (encoder_) {
        opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(bitrate_bps));
        ESP_LOGI(TAG, "Bitrate set to %d bps", bitrate_bps);
    }
}
