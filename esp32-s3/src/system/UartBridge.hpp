#pragma once
#include <functional>
#include <atomic>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "UartProtocol.hpp"

// UartBridge for ESP32-S3: receives STATUS_UPDATE from C5, sends CONTROL_CMD to C5.
// Uses UART1 at 460800 baud. Lightweight alternative to SPI for control messages.
class UartBridge {
public:
    struct Config {
        gpio_num_t pin_tx;
        gpio_num_t pin_rx;
        uart_port_t uart_num = UART_NUM_1;
        int baud_rate = 115200;
    };

    using StatusUpdateCb = std::function<void(uint8_t interaction, uint8_t connectivity,
                                               uint8_t system_state, uint8_t emotion)>;
    using ControlCmdCb = std::function<void(uart_proto::ControlCmd cmd, const uint8_t* data, size_t len)>;

    UartBridge() = default;
    ~UartBridge();

    bool init(const Config& cfg);
    void start();
    void stop();

    // Send control command to C5 (non-blocking)
    bool sendControlCmd(uart_proto::ControlCmd cmd,
                        const uint8_t* data = nullptr, size_t len = 0);

    // Callback for status updates received from C5
    void onStatusUpdate(StatusUpdateCb cb) { status_cb_ = std::move(cb); }
    void onControlCmd(ControlCmdCb cb) { ctrl_cb_ = std::move(cb); }

private:
    static void rxTaskEntry(void* arg);
    void rxLoop();

    Config cfg_{};
    std::atomic<bool> started_{false};
    TaskHandle_t rx_task_ = nullptr;
    StatusUpdateCb status_cb_;
    ControlCmdCb ctrl_cb_;
};
