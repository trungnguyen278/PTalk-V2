#pragma once

/**
 * File:    AudioOutput.hpp
 * Author:  Trung Nguyen
 * GitHub:  https://github.com/trungnguyen278/PTalk-V2
 * Date:    30 Jun 2026
 *
 * Description:
 *  - Part of the PTalk-V2 project
 *  - Written and maintained by Trung Nguyen
 */

#include <cstdint>
#include <cstddef>

/**
 * AudioOutput
 * ============================================================================
 * Vai trò:
 *  - Phát PCM thô ra loa (I2S / DAC / PWM / file / test sink)
 *  - KHÔNG decode
 *  - KHÔNG biết network
 *
 * Dòng dữ liệu:
 *   AudioCodec → PCM → AudioOutput → SPEAKER
 */
class AudioOutput {
public:
    virtual ~AudioOutput() = default;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    // Chuẩn bị phát (init DMA, buffer, v.v.)
    virtual bool startPlayback() = 0;

    // Dừng phát
    virtual void stopPlayback() = 0;

    // Flush DMA with silence (override in drivers that use DMA ring buffers)
    virtual void flushSilence() {}

    // Enable TX clock for full-duplex mic sharing (no-op if not applicable)
    virtual void enableTxClock() {}

    // ========================================================================
    // Data write
    // ========================================================================

    /**
     * Write PCM samples to speaker
     * @param data PCM buffer
     * @param len  number of bytes
     * @return bytes actually written
     */
    virtual size_t writePcm(const int16_t* pcm, size_t pcm_samples) = 0;

    // ========================================================================
    // Control
    // ========================================================================

    // Volume 0–100
    virtual void setVolume(uint8_t percent) = 0;

    // Power saving mode
    virtual void setLowPower(bool enable) = 0;

    // ========================================================================
    // Info
    // ========================================================================

    virtual uint32_t sampleRate() const = 0;
    virtual uint8_t  channels() const   = 0;
    virtual uint8_t  bitsPerSample() const = 0;
};
