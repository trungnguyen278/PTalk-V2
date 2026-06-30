#pragma once

/**
 * File:    AppController.hpp
 * Author:  Trung Nguyen
 * GitHub:  https://github.com/trungnguyen278/PTalk-V2
 * Date:    30 Jun 2026
 *
 * Description:
 *  - Part of the PTalk-V2 project
 *  - Written and maintained by Trung Nguyen
 */

#include <memory>
#include <atomic>
#include <cstdint>
#include "system/StateManager.hpp"
#include "system/StateTypes.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace event {
    enum class AppEvent : uint8_t {
        USER_BUTTON,
        RELEASE_BUTTON,
        SLEEP_REQUEST,
        WAKE_REQUEST,
        REBOOT_REQUEST
    };
}

class DisplayManager;
class PowerManager;
class AudioManager;
class SpiBridge;
class UartBridge;
class TouchInput;

// AppController for ESP32-S3: orchestrates Display, Power, Audio, SPI Bridge, Touch.
// S3 now owns audio + button. Interaction state is managed here and sent to C5 via SPI.
class AppController {
public:
    static AppController& instance();

    bool init();
    void start();
    void stop();
    void reboot();

    void postEvent(event::AppEvent evt);

    void attachModules(std::unique_ptr<DisplayManager> displayIn,
                       std::unique_ptr<PowerManager> powerIn,
                       std::unique_ptr<AudioManager> audioIn,
                       std::unique_ptr<SpiBridge> spiIn,
                       std::unique_ptr<UartBridge> uartIn,
                       std::unique_ptr<TouchInput> touchIn);

private:
    AppController() = default;
    ~AppController();
    AppController(const AppController&) = delete;
    AppController& operator=(const AppController&) = delete;

    static void controllerTask(void* param);
    void processQueue();

    void onInteractionStateChanged(state::InteractionState, state::InputSource);
    void onConnectivityStateChanged(state::ConnectivityState);
    void onSystemStateChanged(state::SystemState);
    void onPowerStateChanged(state::PowerState);
    void onEmotionStateChanged(state::EmotionState);

    int sub_inter_id   = -1;
    int sub_conn_id    = -1;
    int sub_sys_id     = -1;
    int sub_power_id   = -1;
    int sub_emotion_id = -1;

    std::unique_ptr<DisplayManager>   display;
    std::unique_ptr<PowerManager>     power;
    std::unique_ptr<AudioManager>     audio;
    std::unique_ptr<SpiBridge>        spi;
    std::unique_ptr<UartBridge>       uart;
    std::unique_ptr<TouchInput>       touch;

    QueueHandle_t app_queue = nullptr;
    TaskHandle_t  app_task  = nullptr;
    std::atomic<bool> started{false};
};
