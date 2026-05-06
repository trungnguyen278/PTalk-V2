#pragma once

// =============================================================================
// PTalk V2 - ESP32-S3 Pin Configuration
// =============================================================================
// Responsibilities: Display, Battery, I2S Audio (Mic+Speaker), Opus codec,
//                   Button, SPI3 master bridge to C5
// =============================================================================

namespace PinConfig {

// --- SPI Display (ST7789 240x320) on SPI2 ---
static constexpr int SPI_MOSI  = 12;
static constexpr int SPI_SCLK  = 11;
static constexpr int LCD_CS    = 10;
static constexpr int LCD_DC    = 13;
static constexpr int LCD_RST   = 14;
static constexpr int LCD_BL    = 21;  // Backlight PWM

// --- Battery ADC ---
static constexpr int BATTERY_ADC = 1;  // ADC1_CH0 = GPIO1

// --- Charge status (optional, GPIO_NUM_NC if not wired) ---
static constexpr int CHG_STATUS = -1;
static constexpr int CHG_FULL   = -1;

// --- Button ---
static constexpr int BUTTON    = 4;   // Changed from 0 (BOOT pin) to GPIO 4

// --- I2S Audio (Full-Duplex, shared BCLK/WS for mic+speaker) ---
static constexpr int I2S_BCLK  = 15;  // Shared bit clock
static constexpr int I2S_WS    = 16;  // Shared word select
static constexpr int I2S_DIN   = 19;  // ICS43434 mic data in
static constexpr int I2S_DOUT  = 20;  // MAX98357 speaker data out
static constexpr int I2S_NUM   = 0;   // I2S0

// --- Speaker (MAX98357) control ---
static constexpr int SPK_SD    = 40;  // MAX98357 shutdown (LOW=off, HIGH=on)
static constexpr int SPK_GAIN  = 41;  // MAX98357 GAIN (LOW=15dB, float=9dB, HIGH=12dB)

// --- UART Bridge to ESP32-C5 (control/status messages) ---
static constexpr int UART_TX       = 18;  // S3 GPIO18 -> C5 GPIO7   (RX)
static constexpr int UART_RX       = 17;  // S3 GPIO17 <- C5 GPIO6   (TX)

// --- SPI3 Bridge to ESP32-C5 (S3=Master, C5=Slave) ---
static constexpr int SPI3_MOSI      = 35;  // S3 GPIO35 -> C5 GPIO2  (MOSI)
static constexpr int SPI3_MISO      = 36;  // S3 GPIO36 <- C5 GPIO3  (MISO)
static constexpr int SPI3_SCLK      = 37;  // S3 GPIO37 -> C5 GPIO4  (SCLK)
static constexpr int SPI3_CS        = 38;  // S3 GPIO38 -> C5 GPIO5  (CS)
static constexpr int SPI3_HANDSHAKE = 39;  // S3 GPIO39 <- C5 GPIO8  (HIGH = C5 has data)

// --- Battery voltage divider resistors (ohms) ---
static constexpr float BAT_R1 = 10000.0f;
static constexpr float BAT_R2 = 20000.0f;

} // namespace PinConfig
