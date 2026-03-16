#include "AppController.hpp"
#include "system/NetworkManager.hpp"
#include "system/AudioManager.hpp"
#include "system/UartBridge.hpp"
#include "TouchInput.hpp"

#include "esp_log.h"
#include <utility>

static const char* TAG = "AppController";

struct AppMessage {
    enum class Type : uint8_t {
        INTERACTION,
        CONNECTIVITY,
        SYSTEM,
        APP_EVENT
    } type;

    state::InteractionState  interaction_state;
    state::InputSource       interaction_source;
    state::ConnectivityState connectivity_state;
    state::SystemState       system_state;
    event::AppEvent          app_event;
};

// === Singleton ===

AppController& AppController::instance()
{
    static AppController inst;
    return inst;
}

AppController::~AppController()
{
    stop();
    auto& sm = StateManager::instance();
    if (sub_inter_id != -1) sm.unsubscribeInteraction(sub_inter_id);
    if (sub_conn_id != -1)  sm.unsubscribeConnectivity(sub_conn_id);
    if (sub_sys_id != -1)   sm.unsubscribeSystem(sub_sys_id);
    if (app_queue) { vQueueDelete(app_queue); app_queue = nullptr; }
}

// === Module attachment ===

void AppController::attachModules(std::unique_ptr<AudioManager> audioIn,
                                   std::unique_ptr<NetworkManager> networkIn,
                                   std::unique_ptr<TouchInput> touchIn,
                                   std::unique_ptr<UartBridge> uartIn)
{
    if (started.load()) return;
    audio   = std::move(audioIn);
    network = std::move(networkIn);
    touch   = std::move(touchIn);
    uart    = std::move(uartIn);
}

// === Lifecycle ===

bool AppController::init()
{
    ESP_LOGI(TAG, "init()");

    app_queue = xQueueCreate(16, sizeof(AppMessage));
    if (!app_queue) {
        ESP_LOGE(TAG, "Failed to create queue");
        return false;
    }

    auto& sm = StateManager::instance();

    sub_inter_id = sm.subscribeInteraction(
        [this](state::InteractionState s, state::InputSource src) {
            AppMessage msg{}; msg.type = AppMessage::Type::INTERACTION;
            msg.interaction_state = s; msg.interaction_source = src;
            if (app_queue) xQueueSend(app_queue, &msg, 0);
        });

    sub_conn_id = sm.subscribeConnectivity(
        [this](state::ConnectivityState s) {
            AppMessage msg{}; msg.type = AppMessage::Type::CONNECTIVITY;
            msg.connectivity_state = s;
            if (app_queue) xQueueSend(app_queue, &msg, 0);
        });

    sub_sys_id = sm.subscribeSystem(
        [this](state::SystemState s) {
            AppMessage msg{}; msg.type = AppMessage::Type::SYSTEM;
            msg.system_state = s;
            if (app_queue) xQueueSend(app_queue, &msg, 0);
        });

    return true;
}

void AppController::start()
{
    if (started.load()) return;
    started.store(true);

    // Start controller task
    xTaskCreatePinnedToCore(&AppController::controllerTask, "AppCtrl", 4096, this, 4, &app_task, 0);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Start modules: Network -> Audio -> Touch -> UART
    if (network) network->start();
    if (audio) audio->start();
    if (touch) touch->start();
    if (uart) uart->start();

    ESP_LOGI(TAG, "AppController started");
}

void AppController::stop()
{
    if (!started.load()) return;
    started.store(false);

    if (audio) audio->stop();
    if (network) network->stop();
    if (touch) touch->stop();
    if (uart) uart->stop();

    vTaskDelay(pdMS_TO_TICKS(100));
    app_task = nullptr;
    ESP_LOGI(TAG, "AppController stopped");
}

void AppController::reboot()
{
    ESP_LOGW(TAG, "Reboot requested");
    esp_restart();
}

void AppController::postEvent(event::AppEvent evt)
{
    if (!app_queue) return;
    AppMessage msg{}; msg.type = AppMessage::Type::APP_EVENT; msg.app_event = evt;
    xQueueSend(app_queue, &msg, 0);
}

// === Emotion parsing ===

state::EmotionState AppController::parseEmotionCode(const std::string& code)
{
    return NetworkManager::parseEmotionCode(code);
}

// === Task & Queue ===

void AppController::controllerTask(void* param)
{
    static_cast<AppController*>(param)->processQueue();
}

void AppController::processQueue()
{
    ESP_LOGI(TAG, "Controller task started");
    AppMessage msg{};

    while (started.load()) {
        if (xQueueReceive(app_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.type) {
            case AppMessage::Type::INTERACTION:
                onInteractionStateChanged(msg.interaction_state, msg.interaction_source);
                break;
            case AppMessage::Type::CONNECTIVITY:
                onConnectivityStateChanged(msg.connectivity_state);
                break;
            case AppMessage::Type::SYSTEM:
                onSystemStateChanged(msg.system_state);
                break;
            case AppMessage::Type::APP_EVENT:
                switch (msg.app_event) {
                case event::AppEvent::USER_BUTTON:
                    if (StateManager::instance().getConnectivityState() != state::ConnectivityState::ONLINE) {
                        ESP_LOGW(TAG, "Button ignored - not online");
                        break;
                    }
                    if (audio && StateManager::instance().getInteractionState() == state::InteractionState::SPEAKING) {
                        audio->stopSpeaking();
                    }
                    StateManager::instance().setInteractionState(
                        state::InteractionState::LISTENING, state::InputSource::BUTTON);
                    break;
                case event::AppEvent::RELEASE_BUTTON:
                    // Always stop listening on release, even if connectivity dropped
                    StateManager::instance().setInteractionState(
                        state::InteractionState::IDLE, state::InputSource::BUTTON);
                    break;
                case event::AppEvent::REBOOT_REQUEST:
                    reboot();
                    break;
                case event::AppEvent::SLEEP_REQUEST:
                case event::AppEvent::WAKE_REQUEST:
                    break;
                }
                break;
            }
        }
    }

    vTaskDelete(nullptr);
}

// === State callbacks ===

void AppController::onInteractionStateChanged(state::InteractionState s, state::InputSource src)
{
    ESP_LOGI(TAG, "Interaction: %d (src=%d)", (int)s, (int)src);

    // Forward state to S3 via UART
    if (uart) {
        uart->sendStatusUpdate(
            (uint8_t)s,
            (uint8_t)StateManager::instance().getConnectivityState(),
            (uint8_t)StateManager::instance().getSystemState());
    }

    switch (s) {
    case state::InteractionState::TRIGGERED:
        StateManager::instance().setInteractionState(state::InteractionState::LISTENING, src);
        break;
    case state::InteractionState::CANCELLING:
        StateManager::instance().setInteractionState(state::InteractionState::IDLE, state::InputSource::UNKNOWN);
        break;
    default:
        break;
    }
}

void AppController::onConnectivityStateChanged(state::ConnectivityState s)
{
    ESP_LOGI(TAG, "Connectivity: %d", (int)s);

    // Forward to S3
    if (uart) {
        uart->sendStatusUpdate(
            (uint8_t)StateManager::instance().getInteractionState(),
            (uint8_t)s,
            (uint8_t)StateManager::instance().getSystemState());
    }

    if (s == state::ConnectivityState::OFFLINE && audio) {
        audio->stopAll();
        StateManager::instance().setInteractionState(
            state::InteractionState::IDLE, state::InputSource::UNKNOWN);
    }
}

void AppController::onSystemStateChanged(state::SystemState s)
{
    ESP_LOGI(TAG, "System: %d", (int)s);

    if (s == state::SystemState::ERROR && audio) {
        audio->stopAll();
        StateManager::instance().setInteractionState(
            state::InteractionState::IDLE, state::InputSource::UNKNOWN);
    }
}
