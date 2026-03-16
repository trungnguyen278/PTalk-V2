#include "AudioManager.hpp"
#include "AudioInput.hpp"
#include "AudioOutput.hpp"
#include "AudioCodec.hpp"
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

    // Create stream buffers (balanced for RAM — keep ~25KB free for WS+MQTT)
    sb_mic_pcm     = xStreamBufferCreate(2 * 1024, 1);
    sb_mic_encoded = xStreamBufferCreate(4 * 1024, 1);    // 4KB mic encoded (was 8KB)
    sb_spk_pcm     = xStreamBufferCreate(4 * 1024, 1);    // 4KB PCM + pre-buffer logic
    sb_spk_encoded = xStreamBufferCreate(4 * 1024, 1);    // 4KB encoded downlink

    if (!sb_mic_pcm || !sb_mic_encoded || !sb_spk_pcm || !sb_spk_encoded) {
        ESP_LOGE(TAG, "Failed to create stream buffers");
        return false;
    }

    // Subscribe to interaction state
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

    // Task doc mic (reads I2S -> sb_mic_pcm)
    xTaskCreatePinnedToCore(&AudioManager::micReadTaskEntry, "MicRead", 3072, this, 6, &mic_read_task, 0);
    // Task Stream mic (encode sb_mic_pcm -> sb_mic_encoded)
    xTaskCreatePinnedToCore(&AudioManager::micStreamTaskEntry, "MicStream", 24576, this, 5, &mic_stream_task, 0);
    // Task Nhan Audio (decode sb_spk_encoded -> sb_spk_pcm)
    xTaskCreatePinnedToCore(&AudioManager::audioRecvTaskEntry, "AudioRecv", 16384, this, 5, &audio_recv_task, 0);
    // Task phat audio (sb_spk_pcm -> I2S TX)
    xTaskCreatePinnedToCore(&AudioManager::spkPlayTaskEntry, "SpkPlay", 3072, this, 6, &spk_play_task, 0);
}

void AudioManager::stop()
{
    if (!started) return;
    started = false;
    ESP_LOGW(TAG, "stop()");
    stopAll();

    // Wait for tasks to exit
    vTaskDelay(pdMS_TO_TICKS(500));
    mic_read_task = nullptr;
    mic_stream_task = nullptr;
    audio_recv_task = nullptr;
    spk_play_task = nullptr;
}

bool AudioManager::allocateResources()
{
    if (sb_mic_pcm != nullptr) return true;
    sb_mic_pcm     = xStreamBufferCreate(2 * 1024, 1);
    sb_mic_encoded = xStreamBufferCreate(4 * 1024, 1);
    sb_spk_pcm     = xStreamBufferCreate(4 * 1024, 1);
    sb_spk_encoded = xStreamBufferCreate(4 * 1024, 1);
    return (sb_mic_pcm && sb_mic_encoded && sb_spk_pcm && sb_spk_encoded);
}

void AudioManager::freeResources()
{
    stop();
    auto del = [](StreamBufferHandle_t& h) { if (h) { vStreamBufferDelete(h); h = nullptr; } };
    del(sb_mic_pcm); del(sb_mic_encoded); del(sb_spk_pcm); del(sb_spk_encoded);
    ESP_LOGI(TAG, "Resources freed");
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
    case state::InteractionState::SLEEPING:    stopAll(); setPowerSaving(true); break;
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

    // Wait for speaker buffers to drain (max 5 seconds)
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
void AudioManager::setPowerSaving(bool enable) { power_saving = enable; if (enable) stopAll(); }

// === Task entries ===

void AudioManager::micReadTaskEntry(void* arg)   { static_cast<AudioManager*>(arg)->micReadLoop(); }
void AudioManager::micStreamTaskEntry(void* arg)  { static_cast<AudioManager*>(arg)->micStreamLoop(); }
void AudioManager::audioRecvTaskEntry(void* arg)   { static_cast<AudioManager*>(arg)->audioRecvLoop(); }
void AudioManager::spkPlayTaskEntry(void* arg)     { static_cast<AudioManager*>(arg)->spkPlayLoop(); }

// === Task doc mic: I2S RX -> sb_mic_pcm ===

void AudioManager::micReadLoop()
{
    ESP_LOGI(TAG, "MicRead task started");
    constexpr size_t PCM_FRAME = 256;
    int16_t pcm_buf[PCM_FRAME];
    uint32_t read_count = 0;
    uint32_t zero_count = 0;

    while (started) {
        if (!listening || power_saving) {
            if (read_count > 0) {
                ESP_LOGI(TAG, "MicRead: session ended, reads=%lu, zeros=%lu",
                         (unsigned long)read_count, (unsigned long)zero_count);
                read_count = 0;
                zero_count = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        size_t samples = input->readPcm(pcm_buf, PCM_FRAME);
        if (samples == 0) {
            zero_count++;
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        read_count++;
        if (read_count == 1 || read_count % 100 == 0) {
            ESP_LOGI(TAG, "MicRead: #%lu, %zu samples, first=%d",
                     (unsigned long)read_count, samples, pcm_buf[0]);
        }

        size_t bytes = samples * sizeof(int16_t);
        size_t sent = xStreamBufferSend(sb_mic_pcm, pcm_buf, bytes, pdMS_TO_TICKS(10));
        if (sent < bytes) {
            ESP_LOGW(TAG, "MicRead: buffer full, dropped %zu bytes", bytes - sent);
        }
    }

    ESP_LOGW(TAG, "MicRead task ended");
    vTaskDelete(nullptr);
}

// === Task Stream mic: sb_mic_pcm -> Encode -> sb_mic_encoded ===

void AudioManager::micStreamLoop()
{
    ESP_LOGI(TAG, "MicStream task started");

    const size_t pcm_frame = codec->pcmFrameSamples();  // 320 for Opus 20ms@16kHz
    const size_t pcm_frame_bytes = pcm_frame * sizeof(int16_t);  // 640 bytes
    const size_t max_encoded = codec->encodedFrameBytes() + 16;
    int16_t* pcm_in = new int16_t[pcm_frame];
    uint8_t* encoded = new uint8_t[max_encoded];
    size_t accum = 0;  // bytes accumulated in pcm_in

    ESP_LOGI(TAG, "MicStream: pcm_frame=%zu samples (%zu bytes)", pcm_frame, pcm_frame_bytes);

    while (started) {
        if (!listening || power_saving) {
            accum = 0;
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // Accumulate partial reads until we have a full Opus frame
        size_t need = pcm_frame_bytes - accum;
        size_t got = xStreamBufferReceive(
            sb_mic_pcm, ((uint8_t*)pcm_in) + accum, need, pdMS_TO_TICKS(20));
        accum += got;

        if (accum >= pcm_frame_bytes) {
            size_t enc_len = codec->encode(pcm_in, pcm_frame, encoded, max_encoded);
            accum = 0;
            if (enc_len > 0) {
                xStreamBufferSend(sb_mic_encoded, encoded, enc_len, pdMS_TO_TICKS(10));
                static uint32_t enc_count = 0;
                enc_count++;
                if (enc_count == 1 || enc_count % 50 == 0) {
                    ESP_LOGI(TAG, "MicStream: encoded #%lu, %zu bytes", (unsigned long)enc_count, enc_len);
                }
            }
        }
    }

    delete[] pcm_in;
    delete[] encoded;
    ESP_LOGW(TAG, "MicStream task ended");
    vTaskDelete(nullptr);
}

// === Task Nhan Audio: sb_spk_encoded -> Decode -> sb_spk_pcm ===

void AudioManager::audioRecvLoop()
{
    ESP_LOGI(TAG, "AudioRecv task started");

    const size_t pcm_frame = codec->pcmFrameSamples();
    constexpr size_t ENC_PREBUF = 1500;  // ~12 frames = 240ms encoded pre-buffer
    uint8_t* frame_data = new uint8_t[300];
    int16_t* pcm_out = new int16_t[pcm_frame];
    bool new_session = true;
    bool enc_prebuffered = false;
    uint32_t decode_count = 0;

    while (started) {
        if (!speaking || power_saving) {
            if (sb_spk_encoded) xStreamBufferReset(sb_spk_encoded);
            if (sb_spk_pcm) xStreamBufferReset(sb_spk_pcm);
            new_session = true;
            enc_prebuffered = false;
            decode_count = 0;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Pre-buffer encoded data before starting decode
        if (!enc_prebuffered) {
            size_t enc_avail = xStreamBufferBytesAvailable(sb_spk_encoded);
            if (enc_avail < ENC_PREBUF) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            enc_prebuffered = true;
            ESP_LOGI(TAG, "AudioRecv: encoded pre-buffer ready (%zu bytes)", enc_avail);
        }

        // Read 2-byte length prefix
        uint8_t header[2];
        size_t got = xStreamBufferReceive(sb_spk_encoded, header, 2, pdMS_TO_TICKS(20));
        if (got < 2) continue;

        uint16_t frame_len = header[0] | ((uint16_t)header[1] << 8);
        if (frame_len == 0 || frame_len > 256) {
            ESP_LOGE(TAG, "AudioRecv: bad frame len %u, resetting", frame_len);
            xStreamBufferReset(sb_spk_encoded);
            continue;
        }

        // Read Opus frame data
        size_t data_got = xStreamBufferReceive(sb_spk_encoded, frame_data, frame_len, pdMS_TO_TICKS(50));
        if (data_got < frame_len) {
            // Stream lost sync — remaining bytes are garbage, reset
            ESP_LOGW(TAG, "AudioRecv: incomplete frame (%zu/%u), resetting stream", data_got, frame_len);
            xStreamBufferReset(sb_spk_encoded);
            continue;
        }

        if (new_session) {
            codec->reset();
            new_session = false;
            ESP_LOGI(TAG, "New decode session started");
        }

        // Decode: build length-prefixed buffer for codec->decode()
        uint8_t decode_buf[258];
        decode_buf[0] = header[0];
        decode_buf[1] = header[1];
        memcpy(decode_buf + 2, frame_data, frame_len);

        uint32_t t0 = (uint32_t)(esp_timer_get_time() / 1000);
        size_t out_samples = codec->decode(decode_buf, 2 + frame_len, pcm_out, pcm_frame);
        uint32_t t1 = (uint32_t)(esp_timer_get_time() / 1000);

        if (out_samples > 0) {
            xStreamBufferSend(sb_spk_pcm, pcm_out, out_samples * sizeof(int16_t), pdMS_TO_TICKS(200));
        }

        decode_count++;
        if (decode_count == 1 || decode_count % 200 == 0) {
            size_t enc_avail = xStreamBufferBytesAvailable(sb_spk_encoded);
            size_t pcm_avail = xStreamBufferBytesAvailable(sb_spk_pcm);
            ESP_LOGI(TAG, "AudioRecv: #%lu, decode=%lums, enc_buf=%zu, pcm_buf=%zu",
                     (unsigned long)decode_count, (unsigned long)(t1 - t0),
                     enc_avail, pcm_avail);
        }

        // Yield to prevent watchdog
        taskYIELD();
    }

    delete[] frame_data;
    delete[] pcm_out;
    ESP_LOGW(TAG, "AudioRecv task ended");
    vTaskDelete(nullptr);
}

// === Task phat audio: sb_spk_pcm -> I2S TX ===

void AudioManager::spkPlayLoop()
{
    ESP_LOGI(TAG, "SpkPlay task started");

    constexpr size_t PCM_CHUNK = 256;
    constexpr size_t PREBUF_BYTES = 3200;  // ~100ms at 16kHz mono 16-bit (5 Opus frames)
    int16_t pcm_chunk[PCM_CHUNK];
    bool i2s_started = false;
    bool prebuffered = false;

    while (started) {
        if (!speaking || power_saving) {
            if (i2s_started) {
                output->stopPlayback();
                i2s_started = false;
            }
            prebuffered = false;
            ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(100));
            continue;
        }

        // Pre-buffer: wait until enough PCM data before starting I2S
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
            ESP_LOGI(TAG, "I2S playback started (SW SINE TEST)");
        }

#if 1  // DEBUG: Integer sine wave test (bypass Opus decode)
        {
            // 440Hz sine at 16kHz: period = 16000/440 ≈ 36.36 samples
            // Pre-computed 37-sample LUT (one full cycle, amplitude ≈ 8000)
            static const int16_t sine_lut[] = {
                0, 1357, 2651, 3825, 4826, 5612, 6147, 6409,
                6389, 6092, 5536, 4749, 3767, 2632, 1393, 100,
                -1196, -2441, -3581, -4568, -5357, -5913, -6214, -6248,
                -6016, -5531, -4815, -3900, -2826, -1636, -379, 896,
                2136, 3289, 4306, 5143, 5765, 6143
            };
            static const size_t LUT_LEN = sizeof(sine_lut) / sizeof(sine_lut[0]);
            static size_t lut_idx = 0;

            for (size_t i = 0; i < PCM_CHUNK; ++i) {
                pcm_chunk[i] = sine_lut[lut_idx];
                if (++lut_idx >= LUT_LEN) lut_idx = 0;
            }
            size_t samples = PCM_CHUNK;
            // Drain the stream buffer so it doesn't fill up
            while (xStreamBufferBytesAvailable(sb_spk_pcm) > 0) {
                int16_t tmp[64];
                xStreamBufferReceive(sb_spk_pcm, tmp, sizeof(tmp), 0);
            }
            uint32_t t0 = (uint32_t)(esp_timer_get_time() / 1000);
            output->writePcm(pcm_chunk, samples);
            uint32_t t1 = (uint32_t)(esp_timer_get_time() / 1000);
            size_t got = samples * sizeof(int16_t);
#else  // Normal: read from stream buffer
        size_t got = xStreamBufferReceive(
            sb_spk_pcm, pcm_chunk, sizeof(pcm_chunk), pdMS_TO_TICKS(100));

        if (got >= sizeof(int16_t)) {
            size_t samples = got / sizeof(int16_t);
            uint32_t t0 = (uint32_t)(esp_timer_get_time() / 1000);
            output->writePcm(pcm_chunk, samples);
            uint32_t t1 = (uint32_t)(esp_timer_get_time() / 1000);
#endif

            static uint32_t total_samples = 0;
            static uint32_t start_ms = 0;
            static uint32_t write_count = 0;
            if (total_samples == 0) start_ms = t0;
            total_samples += samples;
            write_count++;

            // Log every 16000 samples (~1 second of audio)
            if (total_samples >= 16000 && write_count % 63 == 0) {
                uint32_t elapsed = t1 - start_ms;
                uint32_t expected = (total_samples * 1000) / 16000;
                ESP_LOGI(TAG, "SpkPlay: %lu samples in %lums (expected %lums), write=%lums, got=%zu",
                         (unsigned long)total_samples, (unsigned long)elapsed,
                         (unsigned long)expected, (unsigned long)(t1 - t0), got);
            }
        }
    }

    if (i2s_started) output->stopPlayback();
    ESP_LOGW(TAG, "SpkPlay task ended");
    vTaskDelete(nullptr);
}
