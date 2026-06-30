#pragma once

/**
 * File:    StateTypes.hpp
 * Author:  Trung Nguyen
 * GitHub:  https://github.com/trungnguyen278/PTalk-V2
 * Date:    30 Jun 2026
 *
 * Description:
 *  - Part of the PTalk-V2 project
 *  - Written and maintained by Trung Nguyen
 */

#include <cstdint>

namespace state
{

    // ---------- Interaction (S3 owns: has button+audio) ----------
    enum class InteractionState : uint8_t
    {
        IDLE,
        TRIGGERED,
        LISTENING,
        PROCESSING,
        SPEAKING,
        CANCELLING,
        MUTED,
        SLEEPING
    };

    // ---------- Connectivity (mirrored from C5 via SPI) ----------
    enum class ConnectivityState : uint8_t
    {
        OFFLINE,
        CONNECTING_WIFI,
        WIFI_CONNECTED,
        CONNECTING_WS,
        ONLINE
    };

    // ---------- System ----------
    enum class SystemState : uint8_t
    {
        BOOTING,
        RUNNING,
        ERROR,
        MAINTENANCE
    };

    // ---------- Power (S3 manages battery) ----------
    enum class PowerState : uint8_t
    {
        NORMAL,
        LOW_BATTERY,
        CRITICAL,
        CHARGING,
        FULL
    };

    // ---------- Input Source ----------
    enum class InputSource : uint8_t
    {
        BUTTON,
        SERVER_COMMAND,
        SYSTEM,
        UNKNOWN
    };

    // ---------- Emotion (received from C5 via SPI) ----------
    enum class EmotionState : uint8_t
    {
        NEUTRAL,
        HAPPY,
        SAD,
        ANGRY,
        CONFUSED,
        EXCITED,
        CALM,
        THINKING
    };

} // namespace state
