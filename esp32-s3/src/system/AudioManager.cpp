#include "AudioManager.hpp"
#include "AudioInput.hpp"
#include "AudioOutput.hpp"
#include "AudioCodec.hpp"
#include "SpiBridge.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>

static const char *TAG = "AudioManager";

AudioManager::AudioManager() = default;

AudioManager::~AudioManager()
{
    stop();
}

// === Dependency injection ===

void AudioManager::setInput(std::unique_ptr<AudioInput> in)   { input = std::move(in); }
void AudioManager::setOutput(std::unique_ptr<AudioOutput> out) { output = std::move(out); }
void AudioManager::setCodec(std::unique_ptr<AudioCodec> cdc)  { codec = std::move(cdc); }
void AudioManager::setSpiBridge(SpiBridge* bridge)             { spi_bridge_ = bridge; }

void AudioManager::setVolume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    if (output) {
        output->setVolume(percent);
        ESP_LOGI(TAG, "Volume set to %u%%", (unsigned)percent);
    }
}

// === Init / Start / Stop ===

bool AudioManager::init()
{
    ESP_LOGI(TAG, "init()");

    if (!input || !output || !codec) {
        ESP_LOGE(TAG, "Missing input/output/codec");
        return false;
    }

    if (!input->init()) {
        ESP_LOGE(TAG, "Failed to init Audio Input");
        return false;
    }

    // S3 has PSRAM — use larger buffers than C5
    // sb_mic_encoded is unused (MicStream sends directly to SPI, no intermediate buffer)
    // Speaker encoded buffer is 16KB to absorb SPI jitter from C5 relay
    sb_mic_pcm     = xStreamBufferCreate(4 * 1024, 1);
    sb_spk_pcm     = xStreamBufferCreate(8 * 1024, 1);
    sb_spk_encoded = xStreamBufferCreate(16 * 1024, 1);

    if (!sb_mic_pcm || !sb_spk_pcm || !sb_spk_encoded) {
        ESP_LOGE(TAG, "Failed to create stream buffers");
        return false;
    }

    sub_interaction_id = StateManager::instance().subscribeInteraction(
        [this](state::InteractionState s, state::InputSource src) {
            this->handleInteractionState(s, src);
        });

    ESP_LOGI(TAG, "AudioManager init OK");
    return true;
}

void AudioManager::start()
{
    if (started) return;
    started = true;
    ESP_LOGI(TAG, "start()");

    // Use Core 1 for audio tasks on S3 (Core 0 for display/system)
    xTaskCreatePinnedToCore(&AudioManager::micReadTaskEntry,   "MicRead",   3072,  this, 6, &mic_read_task,   1);
    xTaskCreatePinnedToCore(&AudioManager::micStreamTaskEntry, "MicStream", 28672, this, 5, &mic_stream_task, 1);
    xTaskCreatePinnedToCore(&AudioManager::audioRecvTaskEntry, "AudioRecv", 16384, this, 5, &audio_recv_task, 1);
    xTaskCreatePinnedToCore(&AudioManager::spkPlayTaskEntry,   "SpkPlay",   3072,  this, 6, &spk_play_task,   1);
}

void AudioManager::stop()
{
    if (!started) return;
    started = false;
    ESP_LOGW(TAG, "stop()");
    stopAll();
    vTaskDelay(pdMS_TO_TICKS(500));
    mic_read_task = nullptr;
    mic_stream_task = nullptr;
    audio_recv_task = nullptr;
    spk_play_task = nullptr;
}

// === State handling ===

void AudioManager::handleInteractionState(state::InteractionState s, state::InputSource src)
{
    switch (s) {
    case state::InteractionState::LISTENING:   startListening(src); break;
    case state::InteractionState::PROCESSING:  pauseListening(); break;
    case state::InteractionState::SPEAKING:    startSpeaking(); break;
    case state::InteractionState::CANCELLING:
    case state::InteractionState::IDLE:        stopAll(); break;
    case state::InteractionState::SLEEPING:    stopAll(); break;
    default: break;
    }
}

// === Audio actions ===

void AudioManager::startListening(state::InputSource src)
{
    if (listening) return;
    ESP_LOGI(TAG, "Start listening");

    if (speaking) stopSpeaking();
    xStreamBufferReset(sb_spk_encoded);
    xStreamBufferReset(sb_spk_pcm);
    if (codec) codec->reset();

    current_source = src;
    listening = true;
    speaking = false;
    input->startCapture();
}

void AudioManager::pauseListening()
{
    if (!listening) return;
    ESP_LOGI(TAG, "Pause listening");
    input->stopCapture();
}

void AudioManager::stopListening()
{
    if (!listening) return;
    ESP_LOGI(TAG, "Stop listening");
    listening = false;
    input->stopCapture();
    if (sb_mic_pcm) xStreamBufferReset(sb_mic_pcm);
    if (codec) codec->reset();
}

void AudioManager::startSpeaking()
{
    if (speaking) return;
    ESP_LOGI(TAG, "Start speaking");
    speaking = true;
    if (spk_play_task) xTaskNotifyGive(spk_play_task);
}

void AudioManager::stopSpeaking()
{
    if (!speaking) return;
    ESP_LOGI(TAG, "Stop speaking - draining buffers");

    for (int i = 0; i < 250; ++i) {
        size_t enc_bytes = sb_spk_encoded ? xStreamBufferBytesAvailable(sb_spk_encoded) : 0;
        size_t pcm_bytes = sb_spk_pcm ? xStreamBufferBytesAvailable(sb_spk_pcm) : 0;
        if (enc_bytes == 0 && pcm_bytes == 0) break;
        if (i % 50 == 0) {
            ESP_LOGI(TAG, "Draining: enc=%zu pcm=%zu bytes", enc_bytes, pcm_bytes);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    speaking = false;
    if (output) output->stopPlayback();
    if (sb_spk_encoded) xStreamBufferReset(sb_spk_encoded);
    if (sb_spk_pcm) xStreamBufferReset(sb_spk_pcm);
    if (codec) codec->reset();
    ESP_LOGI(TAG, "Speaking stopped");
}

void AudioManager::stopAll()  { stopListening(); stopSpeaking(); }

// === Task entries ===

void AudioManager::micReadTaskEntry(void* arg)   { static_cast<AudioManager*>(arg)->micReadLoop(); }
void AudioManager::micStreamTaskEntry(void* arg)  { static_cast<AudioManager*>(arg)->micStreamLoop(); }
void AudioManager::audioRecvTaskEntry(void* arg)   { static_cast<AudioManager*>(arg)->audioRecvLoop(); }
void AudioManager::spkPlayTaskEntry(void* arg)     { static_cast<AudioManager*>(arg)->spkPlayLoop(); }

// === MicRead: I2S RX -> sb_mic_pcm ===

void AudioManager::micReadLoop()
{
    ESP_LOGI(TAG, "MicRead task started");
    constexpr size_t PCM_FRAME = 256;
    int16_t pcm_buf[PCM_FRAME];
    uint32_t read_count = 0;

    while (started) {
        if (!listening) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        size_t samples = input->readPcm(pcm_buf, PCM_FRAME);
        if (samples == 0) {
            vTaskDelay(1);
            continue;
        }

        read_count++;
        if (read_count == 1 || read_count % 1000 == 0) {
            ESP_LOGI(TAG, "MicRead: #%lu, %zu samples", (unsigned long)read_count, samples);
        }

        size_t bytes = samples * sizeof(int16_t);
        xStreamBufferSend(sb_mic_pcm, pcm_buf, bytes, pdMS_TO_TICKS(10));
    }

    ESP_LOGW(TAG, "MicRead task ended");
    vTaskDelete(nullptr);
}

// === MicStream: sb_mic_pcm -> Encode -> SpiBridge uplink ===

void AudioManager::micStreamLoop()
{
    ESP_LOGI(TAG, "MicStream task started");

    const size_t pcm_frame = codec->pcmFrameSamples();
    const size_t pcm_frame_bytes = pcm_frame * sizeof(int16_t);
    const size_t max_encoded = codec->encodedFrameBytes() + 16;
    int16_t* pcm_in = new int16_t[pcm_frame];
    uint8_t* encoded = new uint8_t[max_encoded];
    size_t accum = 0;

    ESP_LOGI(TAG, "MicStream: pcm_frame=%zu samples (%zu bytes)", pcm_frame, pcm_frame_bytes);

    while (started) {
        if (!listening) {
            accum = 0;
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        size_t need = pcm_frame_bytes - accum;
        size_t got = xStreamBufferReceive(sb_mic_pcm, ((uint8_t*)pcm_in) + accum, need, pdMS_TO_TICKS(20));
        accum += got;

        if (accum >= pcm_frame_bytes) {
            size_t enc_len = codec->encode(pcm_in, pcm_frame, encoded, max_encoded);
            accum = 0;
            if (enc_len > 0 && spi_bridge_) {
                // OpusCodec::encode() already returns [2B LE len][opus data]
                // Send directly — C5 relays to WS tx_buffer as-is
                if (enc_len <= spi_proto::MAX_PAYLOAD) {
                    spi_bridge_->sendAudioUplink(encoded, enc_len);
                }
            }
            // Yield after each encode so IDLE1 can reset the watchdog.
            // Opus encode at complexity 3 takes several ms — without this,
            // back-to-back encodes starve IDLE1 on Core 1.
            taskYIELD();
        }
    }

    delete[] pcm_in;
    delete[] encoded;
    ESP_LOGW(TAG, "MicStream task ended");
    vTaskDelete(nullptr);
}

// === AudioRecv: sb_spk_encoded -> Decode -> sb_spk_pcm ===

void AudioManager::audioRecvLoop()
{
    ESP_LOGI(TAG, "AudioRecv task started");

    const size_t pcm_frame = codec->pcmFrameSamples();
    constexpr size_t ENC_PREBUF = 2000;
    uint8_t* frame_data = new uint8_t[1024];  // Server may send frames >500B at higher bitrates
    int16_t* pcm_out = new int16_t[pcm_frame];
    bool new_session = true;
    bool enc_prebuffered = false;
    uint32_t decode_count = 0;

    while (started) {
        if (!speaking) {
            if (sb_spk_encoded) xStreamBufferReset(sb_spk_encoded);
            if (sb_spk_pcm) xStreamBufferReset(sb_spk_pcm);
            new_session = true;
            enc_prebuffered = false;
            decode_count = 0;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (!enc_prebuffered) {
            size_t enc_avail = xStreamBufferBytesAvailable(sb_spk_encoded);
            if (enc_avail < ENC_PREBUF) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            enc_prebuffered = true;
            ESP_LOGI(TAG, "AudioRecv: encoded pre-buffer ready (%zu bytes)", enc_avail);
        }

        // Read 2-byte header. With raw byte streaming from SPI, header bytes
        // may arrive across two SPI chunks, so accumulate with a loop.
        uint8_t header[2];
        size_t h_got = 0;
        TickType_t h_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(100);
        while (h_got < 2) {
            TickType_t now = xTaskGetTickCount();
            if (now >= h_deadline) break;
            size_t r = xStreamBufferReceive(sb_spk_encoded, header + h_got, 2 - h_got, h_deadline - now);
            if (r == 0) break;
            h_got += r;
        }
        if (h_got < 2) continue;

        uint16_t frame_len = header[0] | ((uint16_t)header[1] << 8);
        if (frame_len == 0 || frame_len > 960) {
            // Bad frame length — likely stream corruption. Skip this header pair.
            ESP_LOGE(TAG, "AudioRecv: bad frame len %u, skipping", frame_len);
            // Put header[1] back as potential start of next header by treating
            // header[0] as junk. Read one more byte into header[0] and retry.
            // Since stream buffer can't "unread", just skip this pair and
            // hope the next 2 bytes form a valid header.
            continue;
        }

        // Read payload — loop until we get all bytes or timeout.
        // With raw SPI streaming, payload may arrive across multiple SPI
        // transactions (each delivering up to 250 bytes every few ms).
        size_t data_got = 0;
        TickType_t d_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(500);
        while (data_got < frame_len) {
            TickType_t now = xTaskGetTickCount();
            if (now >= d_deadline) break;
            size_t r = xStreamBufferReceive(sb_spk_encoded, frame_data + data_got,
                                             frame_len - data_got, d_deadline - now);
            if (r == 0) break;
            data_got += r;
        }
        if (data_got < frame_len) {
            // Incomplete frame — DON'T reset buffer! The remaining bytes are
            // valid data for subsequent frames. Just discard this partial frame.
            ESP_LOGW(TAG, "AudioRecv: incomplete frame (%zu/%u), discarding", data_got, frame_len);
            continue;
        }

        if (new_session) {
            codec->reset();
            new_session = false;
            ESP_LOGI(TAG, "New decode session started");
        }

        size_t out_samples = codec->decode(frame_data, frame_len, pcm_out, pcm_frame);
        if (out_samples > 0) {
            // Opus already handles frame continuity internally via its overlap-add
            // windowing. The old quadratic crossfade was redundant and introduced
            // artifacts (attenuating the first 80 samples of every frame).
            xStreamBufferSend(sb_spk_pcm, pcm_out, out_samples * sizeof(int16_t), pdMS_TO_TICKS(200));
        }

        decode_count++;
        if (decode_count == 1 || decode_count % 200 == 0) {
            ESP_LOGI(TAG, "AudioRecv: decoded #%lu frames", (unsigned long)decode_count);
        }
        taskYIELD();
    }

    delete[] frame_data;
    delete[] pcm_out;
    ESP_LOGW(TAG, "AudioRecv task ended");
    vTaskDelete(nullptr);
}

// === SpkPlay: sb_spk_pcm -> I2S TX ===

void AudioManager::spkPlayLoop()
{
    ESP_LOGI(TAG, "SpkPlay task started");

    constexpr size_t PCM_CHUNK = 256;
    constexpr size_t PREBUF_BYTES = 3840;
    int16_t pcm_chunk[PCM_CHUNK];
    bool i2s_started = false;
    bool prebuffered = false;

    while (started) {
        if (!speaking) {
            if (i2s_started) {
                output->stopPlayback();
                i2s_started = false;
            }
            prebuffered = false;
            ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(100));
            continue;
        }

        if (!prebuffered) {
            size_t avail = xStreamBufferBytesAvailable(sb_spk_pcm);
            if (avail < PREBUF_BYTES) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            prebuffered = true;
            ESP_LOGI(TAG, "SpkPlay: pre-buffer ready (%zu bytes)", avail);
        }

        if (!i2s_started) {
            if (!output->startPlayback()) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            i2s_started = true;
            ESP_LOGI(TAG, "I2S playback started");
        }

        size_t got = xStreamBufferReceive(sb_spk_pcm, pcm_chunk, sizeof(pcm_chunk), pdMS_TO_TICKS(100));
        if (got >= sizeof(int16_t)) {
            size_t samples = got / sizeof(int16_t);
            output->writePcm(pcm_chunk, samples);
        }
    }

    if (i2s_started) output->stopPlayback();
    ESP_LOGW(TAG, "SpkPlay task ended");
    vTaskDelete(nullptr);
}
