#pragma once
#include <functional>
#include <atomic>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "UartProtocol.hpp"

// UartBridge: UART communication between ESP32-C5 and ESP32-S3
// C5 receives: WiFi config, MQTT config, ADC data, commands from S3
// C5 sends: status updates, display commands (emotion) to S3
class UartBridge {
public:
    struct Config {
        int uart_num    = 1;
        int tx_pin      = 6;
        int rx_pin      = 7;
        int baud_rate   = 115200;
        int rx_buf_size = 1024;
        int tx_buf_size = 512;
    };

    // Callbacks for received data from S3
    using WifiConfigCb  = std::function<void(const std::string& ssid, const std::string& pass)>;
    using MqttConfigCb  = std::function<void(const std::string& uri, const std::string& user, const std::string& pass)>;
    using WsConfigCb    = std::function<void(const std::string& url)>;
    using AdcDataCb     = std::function<void(uint16_t voltage_mv, uint8_t battery_pct, bool charging)>;
    using CommandCb     = std::function<void(uart_proto::Command cmd, const uint8_t* data, size_t len)>;

    UartBridge() = default;
    ~UartBridge();

    bool init(const Config& cfg);
    void start();
    void stop();

    // Register callbacks for incoming messages
    void onWifiConfig(WifiConfigCb cb)  { wifi_cb_ = std::move(cb); }
    void onMqttConfig(MqttConfigCb cb)  { mqtt_cb_ = std::move(cb); }
    void onWsConfig(WsConfigCb cb)      { ws_cb_ = std::move(cb); }
    void onAdcData(AdcDataCb cb)        { adc_cb_ = std::move(cb); }
    void onCommand(CommandCb cb)        { cmd_cb_ = std::move(cb); }

    // Send messages to S3
    bool sendStatusUpdate(uint8_t interaction, uint8_t connectivity, uint8_t system_state);
    bool sendDisplayCmd(uint8_t emotion);
    bool sendConfigRequest();

private:
    static void rxTaskEntry(void* arg);
    void rxTaskLoop();

    void handleFrame(uint8_t type, const uint8_t* payload, uint16_t len);
    bool sendFrame(uint8_t type, const uint8_t* payload, uint16_t len);

    Config cfg_{};
    uart_port_t uart_port_ = (uart_port_t)-1;
    std::atomic<bool> started_{false};
    TaskHandle_t rx_task_ = nullptr;

    // Callbacks
    WifiConfigCb  wifi_cb_;
    MqttConfigCb  mqtt_cb_;
    WsConfigCb    ws_cb_;
    AdcDataCb     adc_cb_;
    CommandCb     cmd_cb_;
};
