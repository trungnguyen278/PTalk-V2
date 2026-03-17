#pragma once
#include <functional>
#include <atomic>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "SpiProtocol.hpp"

// SpiBridge for ESP32-S3: SPI3 master communicating with ESP32-C5 slave.
// Polls via handshake GPIO or when S3 has data to send.
// Full-duplex 256-byte transactions.
class SpiBridge {
public:
    struct Config {
        gpio_num_t pin_mosi;      // SPI3 MOSI (to C5)
        gpio_num_t pin_miso;      // SPI3 MISO (from C5)
        gpio_num_t pin_sclk;      // SPI3 SCLK
        gpio_num_t pin_cs;        // SPI3 CS
        gpio_num_t pin_handshake; // Input from C5 (HIGH = C5 has data)
        int clock_speed_hz = 10 * 1000 * 1000; // 10 MHz
    };

    using AudioDownlinkCb = std::function<void(const uint8_t* data, size_t len)>;
    using StatusUpdateCb  = std::function<void(uint8_t interaction, uint8_t connectivity,
                                               uint8_t system_state, uint8_t emotion)>;

    SpiBridge() = default;
    ~SpiBridge();

    bool init(const Config& cfg);
    void start();
    void stop();

    // Send audio uplink (Opus frames) to C5
    bool sendAudioUplink(const uint8_t* data, size_t len);

    // Send control command to C5
    bool sendControlCmd(spi_proto::ControlCmd cmd, const uint8_t* data = nullptr, size_t len = 0);

    // Callbacks for data received from C5
    void onAudioDownlink(AudioDownlinkCb cb) { audio_dl_cb_ = std::move(cb); }
    void onStatusUpdate(StatusUpdateCb cb)   { status_cb_ = std::move(cb); }

private:
    static void pollTaskEntry(void* arg);
    void pollLoop();

    bool doTransaction(const uint8_t* tx_buf, uint8_t* rx_buf);
    void handleRxFrame(const uint8_t* rx_buf);

    Config cfg_{};
    spi_device_handle_t spi_dev_ = nullptr;
    std::atomic<bool> started_{false};
    TaskHandle_t poll_task_ = nullptr;

    // TX queue: frames waiting to be sent
    QueueHandle_t tx_queue_ = nullptr;

    uint8_t tx_seq_ = 0;

    AudioDownlinkCb audio_dl_cb_;
    StatusUpdateCb  status_cb_;
};
