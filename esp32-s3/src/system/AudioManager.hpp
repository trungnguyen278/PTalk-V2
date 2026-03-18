#pragma once
#include <memory>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "system/StateTypes.hpp"
#include "system/StateManager.hpp"

class AudioInput;
class AudioOutput;
class AudioCodec;
class SpiBridge;

// Audio pipeline manager for ESP32-S3.
// Same 4-task architecture as the C5 version, but uplink/downlink go through
// SpiBridge (SPI to C5) instead of directly to WebSocket.
//
// Tasks:
//   - MicRead:    I2S RX -> sb_mic_pcm
//   - MicStream:  sb_mic_pcm -> Encode -> sb_mic_encoded -> SpiBridge uplink
//   - AudioRecv:  sb_spk_encoded (from SpiBridge downlink) -> Decode -> sb_spk_pcm
//   - SpkPlay:    sb_spk_pcm -> I2S TX
class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    // Lifecycle
    bool init();
    void start();
    void stop();

    // Dependency injection
    void setInput(std::unique_ptr<AudioInput> in);
    void setOutput(std::unique_ptr<AudioOutput> out);
    void setCodec(std::unique_ptr<AudioCodec> cdc);
    void setSpiBridge(SpiBridge* bridge);

    // Stream buffer access (for SpiBridge downlink writing)
    StreamBufferHandle_t getSpeakerEncodedBuffer() const { return sb_spk_encoded; }

    // Control
    void setVolume(uint8_t percent);

    // Audio actions
    void startListening(state::InputSource src);
    void pauseListening();
    void stopAll();
    void stopListening();
    void startSpeaking();
    void stopSpeaking();

private:
    void handleInteractionState(state::InteractionState s, state::InputSource src);

    // Task entries
    static void micReadTaskEntry(void* arg);
    static void micStreamTaskEntry(void* arg);
    static void audioRecvTaskEntry(void* arg);
    static void spkPlayTaskEntry(void* arg);

    // Task loops
    void micReadLoop();
    void micStreamLoop();
    void audioRecvLoop();
    void spkPlayLoop();

    // State
    std::atomic<bool> started{false};
    std::atomic<bool> listening{false};
    std::atomic<bool> speaking{false};
    state::InputSource current_source = state::InputSource::UNKNOWN;

    // Components
    std::unique_ptr<AudioInput>  input;
    std::unique_ptr<AudioOutput> output;
    std::unique_ptr<AudioCodec>  codec;
    SpiBridge* spi_bridge_ = nullptr;

    // Stream buffers (sb_mic_encoded removed — MicStream sends directly to SPI)
    StreamBufferHandle_t sb_mic_pcm     = nullptr;
    StreamBufferHandle_t sb_spk_pcm     = nullptr;
    StreamBufferHandle_t sb_spk_encoded = nullptr;

    // Tasks
    TaskHandle_t mic_read_task   = nullptr;
    TaskHandle_t mic_stream_task = nullptr;
    TaskHandle_t audio_recv_task = nullptr;
    TaskHandle_t spk_play_task   = nullptr;

    int sub_interaction_id = -1;
};
