/**
 * File:    CliConsole.cpp
 * Author:  Trung Nguyen
 * GitHub:  https://github.com/trungnguyen278/PTalk-V2
 * Date:    30 Jun 2026
 *
 * Description:
 *  - Part of the PTalk-V2 project
 *  - Written and maintained by Trung Nguyen
 */

#include "system/CliConsole.hpp"
#include "system/StateManager.hpp"
#include "system/StateTypes.hpp"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

#include <cstring>
#include <string>
#include <vector>

static const char* TAG = "CLI";

// ── NVS helpers ─────────────────────────────────────────────────────────────

static void nvs_put(const char* key, const char* val) {
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, key, val);
    nvs_commit(h);
    nvs_close(h);
}

static std::string nvs_get(const char* key) {
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READONLY, &h) != ESP_OK) return {};
    size_t len = 0;
    if (nvs_get_str(h, key, nullptr, &len) != ESP_OK || len == 0) { nvs_close(h); return {}; }
    std::string out(len, '\0');
    nvs_get_str(h, key, out.data(), &len);
    nvs_close(h);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

// ── Tokenizer ───────────────────────────────────────────────────────────────

static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string tok;
    for (char c : line) {
        if (c == ' ' || c == '\t') {
            if (!tok.empty()) { tokens.push_back(tok); tok.clear(); }
        } else {
            tok += c;
        }
    }
    if (!tok.empty()) tokens.push_back(tok);
    return tokens;
}

// ── Command handlers ────────────────────────────────────────────────────────

static void cmd_help() {
    ESP_LOGI(TAG, "──── Commands ────");
    ESP_LOGI(TAG, "  help                Show this help");
    ESP_LOGI(TAG, "  status              Show device status & heap");
    ESP_LOGI(TAG, "  config              Show all NVS config");
    ESP_LOGI(TAG, "  set wifi <s> [p]    Set WiFi SSID & password");
    ESP_LOGI(TAG, "  set ws <url>        Set WebSocket URL");
    ESP_LOGI(TAG, "  set mqtt <url>      Set MQTT URL");
    ESP_LOGI(TAG, "  set user <id> <key> Set user_id & tx_key");
    ESP_LOGI(TAG, "  set name <name>     Set device name");
    ESP_LOGI(TAG, "  erase_nvs           Erase all NVS & restart");
    ESP_LOGI(TAG, "  restart             Restart device");
}

static void cmd_restart() {
    ESP_LOGW(TAG, "Restarting...");
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
}

static void cmd_erase_nvs() {
    ESP_LOGW(TAG, "Erasing all NVS data...");
    nvs_flash_erase();
    ESP_LOGW(TAG, "NVS erased. Restarting...");
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
}

static void cmd_status() {
    auto& sm = StateManager::instance();

    const char* interaction_names[] = {
        "IDLE", "TRIGGERED", "LISTENING", "PROCESSING",
        "SPEAKING", "CANCELLING", "MUTED", "SLEEPING"
    };
    const char* conn_names[] = {
        "OFFLINE", "CONNECTING_WIFI", "WIFI_CONNECTED",
        "CONNECTING_WS", "ONLINE"
    };
    const char* sys_names[] = { "BOOTING", "RUNNING", "ERROR", "MAINTENANCE" };
    const char* emo_names[] = {
        "NEUTRAL", "HAPPY", "SAD", "ANGRY",
        "CONFUSED", "EXCITED", "CALM", "THINKING"
    };

    ESP_LOGI(TAG, "──── Status ────");
    ESP_LOGI(TAG, "  Free heap  : %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "  Min heap   : %lu bytes", (unsigned long)esp_get_minimum_free_heap_size());
    ESP_LOGI(TAG, "  Interaction: %s", interaction_names[(int)sm.getInteractionState()]);
    ESP_LOGI(TAG, "  Connectivity: %s", conn_names[(int)sm.getConnectivityState()]);
    ESP_LOGI(TAG, "  System     : %s", sys_names[(int)sm.getSystemState()]);
    ESP_LOGI(TAG, "  Emotion    : %s", emo_names[(int)sm.getEmotionState()]);
}

static void cmd_config() {
    ESP_LOGI(TAG, "──── NVS Config ────");
    ESP_LOGI(TAG, "  device_name: %s", nvs_get("device_name").c_str());
    ESP_LOGI(TAG, "  ssid       : %s", nvs_get("ssid").c_str());
    ESP_LOGI(TAG, "  pass       : %s", nvs_get("pass").empty() ? "<empty>" : "****");
    ESP_LOGI(TAG, "  ws_url     : %s", nvs_get("ws_url").c_str());
    ESP_LOGI(TAG, "  mqtt_url   : %s", nvs_get("mqtt_url").c_str());
    ESP_LOGI(TAG, "  user_id    : %s", nvs_get("user_id").c_str());
    ESP_LOGI(TAG, "  tx_key     : %s", nvs_get("tx_key").empty() ? "<empty>" : "****");
}

static void cmd_set(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        ESP_LOGW(TAG, "Usage: set wifi|ws|mqtt|user|name <value...>");
        return;
    }
    const auto& sub = args[1];

    if (sub == "wifi") {
        nvs_put("ssid", args[2].c_str());
        nvs_put("pass", args.size() >= 4 ? args[3].c_str() : "");
        ESP_LOGI(TAG, "WiFi saved: ssid='%s'. Restart to apply.", args[2].c_str());
    } else if (sub == "ws") {
        nvs_put("ws_url", args[2].c_str());
        ESP_LOGI(TAG, "WS URL saved: '%s'. Restart to apply.", args[2].c_str());
    } else if (sub == "mqtt") {
        nvs_put("mqtt_url", args[2].c_str());
        ESP_LOGI(TAG, "MQTT URL saved: '%s'. Restart to apply.", args[2].c_str());
    } else if (sub == "user") {
        if (args.size() < 4) {
            ESP_LOGW(TAG, "Usage: set user <id> <key>");
            return;
        }
        nvs_put("user_id", args[2].c_str());
        nvs_put("tx_key", args[3].c_str());
        ESP_LOGI(TAG, "User saved: id='%s'. Restart to apply.", args[2].c_str());
    } else if (sub == "name") {
        nvs_put("device_name", args[2].c_str());
        ESP_LOGI(TAG, "Device name saved: '%s'. Restart to apply.", args[2].c_str());
    } else {
        ESP_LOGW(TAG, "Unknown: set %s", sub.c_str());
    }
}

// ── Dispatch ────────────────────────────────────────────────────────────────

static void dispatch(const std::string& line) {
    auto args = tokenize(line);
    if (args.empty()) return;

    const auto& cmd = args[0];

    if (cmd == "help" || cmd == "?")       cmd_help();
    else if (cmd == "restart")             cmd_restart();
    else if (cmd == "erase_nvs")           cmd_erase_nvs();
    else if (cmd == "status")              cmd_status();
    else if (cmd == "config")              cmd_config();
    else if (cmd == "set")                 cmd_set(args);
    else ESP_LOGW(TAG, "Unknown command: '%s'. Type 'help' for list.", cmd.c_str());
}

// ── UART0 read task ─────────────────────────────────────────────────────────

static void cli_task(void*) {
    uart_config_t uart_cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_NUM_0, &uart_cfg);
    uart_driver_install(UART_NUM_0, 256, 0, 0, nullptr, 0);

    std::string line;
    uint8_t ch;

    while (true) {
        int len = uart_read_bytes(UART_NUM_0, &ch, 1, portMAX_DELAY);
        if (len <= 0) continue;

        if (ch == '\n' || ch == '\r') {
            if (!line.empty()) {
                ESP_LOGI(TAG, "> %s", line.c_str());
                dispatch(line);
                line.clear();
            }
        } else if (ch == 0x7f || ch == '\b') {
            if (!line.empty()) line.pop_back();
        } else if (ch >= 0x20) {
            line += (char)ch;
        }
    }
}

void CliConsole::init() {
    xTaskCreate(cli_task, "cli", 4096, nullptr, 2, nullptr);
    ESP_LOGI(TAG, "CLI ready (type 'help')");
}
