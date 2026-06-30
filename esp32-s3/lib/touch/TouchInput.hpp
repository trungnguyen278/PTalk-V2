#pragma once

/**
 * File:    TouchInput.hpp
 * Author:  Trung Nguyen
 * GitHub:  https://github.com/trungnguyen278/PTalk-V2
 * Date:    30 Jun 2026
 *
 * Description:
 *  - Part of the PTalk-V2 project
 *  - Written and maintained by Trung Nguyen
 */

#include <cstdint>
#include <functional>
#include <atomic>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

class TouchInput
{
public:
    enum class Event : uint8_t
    {
        PRESS,
        RELEASE,
        LONG_PRESS
    };

    struct Config
    {
        gpio_num_t pin;
        bool active_low = true;
        uint32_t long_press_ms = 1500;
        uint32_t debounce_ms = 30;
    };

public:
    TouchInput() = default;
    ~TouchInput();

    //======= Lifecycle =======
    bool init(const Config &cfg);
    void start();
    void stop();

    //======= Event callback =======
    /**
     * Register event callback
     * @param cb Callback function receiving Event
     */
    void onEvent(std::function<void(Event)> cb);

private:
    static void taskEntry(void *arg);
    void loop();

    //======= Read raw input =======
    /**
     * Read raw input state (without debounce)
     * @return true = pressed, false = released
     */
    bool readRaw() const;

private:
    Config cfg_{};
    std::function<void(Event)> cb_;

    TaskHandle_t task_ = nullptr;
    std::atomic<bool> running{false};

    // debounce & timing
    bool last_state = false;
    TickType_t press_tick = 0;
};
