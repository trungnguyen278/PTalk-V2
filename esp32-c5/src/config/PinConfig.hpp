#pragma once

// =============================================================================
// PTalk V2 - ESP32-C5 Pin Configuration
// =============================================================================
// ESP32-C5 has GPIO0-GPIO14 available
// Adjust these pins to match your PCB layout
// =============================================================================

namespace PinConfig {

// --- I2S Audio ---
// ESP32-C5 chỉ có 1 I2S port → full-duplex bắt buộc shared BCLK/WS
// Mic (ICS43434)
static constexpr int MIC_BCLK  = 2;   // ICS43434 SCK  (shared with SPK)
static constexpr int MIC_WS    = 3;   // ICS43434 WS   (shared with SPK)
static constexpr int MIC_DOUT  = 4;   // ICS43434 SD   (mic data out → C5 data in)

// Speaker (MAX98357)
static constexpr int SPK_BCLK  = 2;   // MAX98357 BCLK (shared with MIC)
static constexpr int SPK_WS    = 3;   // MAX98357 LRC  (shared with MIC)
static constexpr int SPK_DIN   = 5;   // MAX98357 DIN  (C5 data out → speaker)

// --- UART (Communication with ESP32-S3) ---
static constexpr int UART_TX   = 6;   // C5 TX -> S3 RX
static constexpr int UART_RX   = 7;   // C5 RX <- S3 TX

// --- Button ---
static constexpr int BUTTON    = 8;   // Push button (active low, pull-up)

// --- UART Configuration ---
static constexpr int UART_NUM       = 1;       // UART1
static constexpr int UART_BAUD_RATE = 115200;

// --- I2S Configuration ---
static constexpr int I2S_NUM        = 0;       // I2S0 (only 1 on C5)

} // namespace PinConfig
