#pragma once

/**
 * File:    PinConfig.hpp
 * Author:  Trung Nguyen
 * GitHub:  https://github.com/trungnguyen278/PTalk-V2
 * Date:    30 Jun 2026
 *
 * Description:
 *  - Part of the PTalk-V2 project
 *  - Written and maintained by Trung Nguyen
 */

// =============================================================================
// PTalk V2 - ESP32-C5 Pin Configuration
// =============================================================================
// ESP32-C5 has GPIO0-GPIO14 available.
// Responsibilities: WiFi, WebSocket, MQTT, SPI slave bridge to S3
// Audio, button, and display are now on S3.
// =============================================================================

namespace PinConfig {

// --- SPI Slave (S3=Master, C5=Slave) ---
static constexpr int SPI_MOSI      = 2;   // C5 GPIO2  <- S3 GPIO35  (MOSI)
static constexpr int SPI_MISO      = 3;   // C5 GPIO3  -> S3 GPIO36  (MISO)
static constexpr int SPI_SCLK      = 4;   // C5 GPIO4  <- S3 GPIO37  (SCLK)
static constexpr int SPI_CS        = 5;   // C5 GPIO5  <- S3 GPIO38  (CS)
static constexpr int SPI_HANDSHAKE = 8;   // C5 GPIO8  -> S3 GPIO39  (HIGH = C5 has data)

// --- SPI Configuration ---
static constexpr int SPI_NUM       = 2;   // SPI2

// --- UART Bridge to S3 (control/status messages) ---
static constexpr int UART_TX       = 7;   // C5 GPIO6  -> S3 GPIO17  (RX)
static constexpr int UART_RX       = 6;   // C5 GPIO7  <- S3 GPIO18  (TX)

// --- NVS Reset Button (hold LOW at boot to erase NVS) ---
// Set to -1 to disable. Assign a real GPIO when hardware is ready.
static constexpr int NVS_RESET_BTN = 9;

// --- Free GPIOs ---
// GPIO 10 is available for future use

} // namespace PinConfig
