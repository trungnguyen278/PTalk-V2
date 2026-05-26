#pragma once
#include <memory>
#include <functional>
#include <string>
#include <atomic>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "system/StateManager.hpp"
#include "system/StateTypes.hpp"

class WifiService;
class WebSocketClient;
class MqttClient;
class SpiBridge;
class UartBridge;
class BluetoothService;

// NetworkManager for ESP32-C5: WiFi + WebSocket + MQTT
// Audio is now on S3. C5 relays audio between SPI and WebSocket.
class NetworkManager {
public:
    struct Config {
        std::string wifi_ssid;
        std::string wifi_pass;
        std::string ws_url;
        std::string mqtt_url;
        std::string user_id;
        std::string tx_key;
    };

    NetworkManager();
    ~NetworkManager();

    bool init(const Config& cfg);
    void start();
    void stop();

    void setCredentials(const std::string& ssid, const std::string& pass);

    // Bridge wiring
    void setSpiBridge(SpiBridge* bridge);
    void setUartBridge(UartBridge* bridge);

    // WebSocket control
    void sendText(const std::string& msg);
    void sendBinary(const uint8_t* data, size_t len);
    void waitTxDrain(uint32_t timeout_ms = 500);
    size_t getWsTxFreeSpace() const;
    void setWSImmuneMode(bool enable);

    // Speaking session tracking
    bool isSpeakingSessionActive() const { return speaking_session_active_; }
    void startSpeakingSession()          { speaking_session_active_ = true; }
    void endSpeakingSession()            { speaking_session_active_ = false; }

    // BLE config mode (triggered by MQTT command)
    void startBleConfigMode();
    void stopBleConfigMode();

    // Emotion parsing
    static state::EmotionState parseEmotionCode(const std::string& code);

private:
    void setupWifi();
    void setupWebSocket();
    void setupMqtt();
    void sendDeviceHandshake();
    void startMqttDeferred();
    static void mqttInitTaskEntry(void* arg);

    void mqttPublishResponse(const char* cmd, const char* status, const char* extra_json = nullptr);
    bool nvsSetU8(const char* key, uint8_t val);
    bool nvsSetString(const char* key, const char* val);

    Config cfg_;

    std::unique_ptr<WifiService>     wifi_;
    std::unique_ptr<WebSocketClient> ws_;
    std::unique_ptr<MqttClient>      mqtt_;
    SpiBridge*  spi_bridge_  = nullptr;
    UartBridge* uart_bridge_ = nullptr;

    // BLE config mode
    BluetoothService* ble_ = nullptr;
    std::atomic<bool> ble_config_active_{false};
    static void bleConfigTaskEntry(void* arg);

    std::atomic<bool> started_{false};
    std::atomic<bool> ws_connected_{false};
    std::atomic<bool> ws_immune_{false};
    std::atomic<bool> speaking_session_active_{false};
    std::atomic<bool> mqtt_init_pending_{false};
};
