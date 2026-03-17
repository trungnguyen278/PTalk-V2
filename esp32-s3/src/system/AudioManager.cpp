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
    // Speaker encoded buffer is 16KB to absorb SPI jitter from C5 relay
    sb_mic_pcm     = xStreamBufferCreate(4 * 1024, 1);
    sb_mic_encoded = xStreamBufferCreate(4 * 1024, 1);
    sb_spk_pcm     = xStreamBufferCreate(8 * 1024, 1);
    sb_spk_encoded = xStreamBufferCreate(16 * 1024, 1);

    if (!sb_mic_pcm || !sb_mic_encoded || !sb_spk_pcm || !sb_spk_encoded) {
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
    if (sb_mic_encoded) xStreamBufferReset(sb_mic_encoded);
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
                // Prepend 2-byte LE length prefix so C5 can relay to WS as-is
                // WS TX loop on C5 expects [2-byte len][opus data] framing
                uint8_t prefixed[spi_proto::MAX_PAYLOAD];
                if (enc_len + 2 <= spi_proto::MAX_PAYLOAD) {
                    prefixed[0] = (uint8_t)(enc_len & 0xFF);
                    prefixed[1] = (uint8_t)((enc_len >> 8) & 0xFF);
                    memcpy(&prefixed[2], encoded, enc_len);
                    spi_bridge_->sendAudioUplink(prefixed, enc_len + 2);
                }
            }
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
    uint8_t* frame_data = new uint8_t[512];
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

        uint8_t header[2];
        size_t got = xStreamBufferReceive(sb_spk_encoded, header, 2, pdMS_TO_TICKS(20));
        if (got < 2) continue;

        uint16_t frame_len = header[0] | ((uint16_t)header[1] << 8);
        if (frame_len == 0 || frame_len > 500) {
            ESP_LOGE(TAG, "AudioRecv: bad frame len %u, resetting", frame_len);
            xStreamBufferReset(sb_spk_encoded);
            continue;
        }

        size_t data_got = xStreamBufferReceive(sb_spk_encoded, frame_data, frame_len, pdMS_TO_TICKS(50));
        if (data_got < frame_len) {
            ESP_LOGW(TAG, "AudioRecv: incomplete frame (%zu/%u), resetting", data_got, frame_len);
            xStreamBufferReset(sb_spk_encoded);
            continue;
        }

        if (new_session) {
            codec->reset();
            new_session = false;
            ESP_LOGI(TAG, "New decode session started");
        }

        size_t out_samples = codec->decode(frame_data, frame_len, pcm_out, pcm_frame);
        if (out_samples > 0) {
            // Quadratic crossfade at frame boundaries
            static int16_t prev_frame_last = 0;
            if (decode_count > 0) {
                const int BLEND = 80;
                int32_t gap = (int32_t)pcm_out[0] - (int32_t)prev_frame_last;
                for (int i = 0; i < BLEND && i < (int)out_samples; ++i) {
                    int32_t t = BLEND - i;
                    int32_t correction = (int32_t)(((int64_t)gap * t * t) / ((int32_t)BLEND * BLEND));
                    int32_t val = (int32_t)pcm_out[i] - correction;
                    if (val > 32767) val = 32767;
                    if (val < -32768) val = -32768;
                    pcm_out[i] = (int16_t)val;
                }
            }
            prev_frame_last = pcm_out[out_samples - 1];
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
