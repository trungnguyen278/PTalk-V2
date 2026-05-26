#include "MqttClient.hpp"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_heap_caps.h"

static const char* TAG = "MqttClient";

MqttClient::MqttClient() = default;

MqttClient::~MqttClient()
{
    stop();
}

// ------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------

void MqttClient::init()
{
    // Nothing heavy here
}

void MqttClient::setUri(const std::string& uri)
{
    uri_ = uri;
}

void MqttClient::setClientId(const std::string& id)
{
    client_id_ = id;
}

void MqttClient::setUsername(const std::string& user)
{
    username_ = user;
}

void MqttClient::setPassword(const std::string& pass)
{
    password_ = pass;
}

void MqttClient::setCredentials(const std::string& user, const std::string& pass)
{
    username_ = user;
    password_ = pass;
}

void MqttClient::start()
{
    if (uri_.empty())
    {
        ESP_LOGE(TAG, "MQTT URI not set");
        return;
    }

    if (client_)
    {
        ESP_LOGW(TAG, "MQTT already started");
        return;
    }

    esp_mqtt_client_config_t cfg = {};

    // Broker
    cfg.broker.address.uri = uri_.c_str();

    // Credentials
    cfg.credentials.client_id = client_id_.empty() ? nullptr : client_id_.c_str();
    cfg.credentials.username = username_.empty() ? nullptr : username_.c_str();
    cfg.credentials.authentication.password = password_.empty() ? nullptr : password_.c_str();

    // Session
    cfg.session.keepalive = 60;
    cfg.session.disable_keepalive = false;

    // Network — disable auto-reconnect entirely. Each failed connect blocks
    // the TCP/lwIP stack for timeout_ms, which can cause WS transport_poll_write
    // to fail → WS disconnects. Let NetworkManager handle retry if needed.
    cfg.network.disable_auto_reconnect = true;
    cfg.network.reconnect_timeout_ms = 60000;
    cfg.network.timeout_ms = 3000;  // 3s connect timeout (fail fast)

    // Buffer
    cfg.buffer.size = 512;
    cfg.buffer.out_size = 256;

    ESP_LOGI(TAG, "Free heap before mqtt_start: %u", esp_get_free_heap_size());

    client_ = esp_mqtt_client_init(&cfg);
    if (!client_)
    {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return;
    }

    esp_err_t err = esp_mqtt_client_register_event(
        client_,
        MQTT_EVENT_ANY,
        &MqttClient::eventHandlerStatic,
        this);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register MQTT events");
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
        return;
    }

    esp_mqtt_client_start(client_);

    ESP_LOGI(TAG, "MQTT started: %s", uri_.c_str());
    ESP_LOGI(TAG, "Free heap after mqtt_start: %u", esp_get_free_heap_size());
}

void MqttClient::stop()
{
    if (!client_)
        return;

    ESP_LOGI(TAG, "Stopping MQTT client");

    esp_mqtt_client_stop(client_);
    esp_mqtt_client_destroy(client_);

    client_ = nullptr;
    connected_ = false;

    if (disconnected_cb_)
        disconnected_cb_();
}

bool MqttClient::publish(const std::string& topic,
                         const std::string& json,
                         int qos,
                         bool retain)
{
    if (!client_ || !connected_) {
        ESP_LOGW(TAG, "PUBLISH FAIL (not connected): topic=%s", topic.c_str());
        return false;
    }

    int msg_id = esp_mqtt_client_publish(
        client_,
        topic.c_str(),
        json.c_str(),
        json.size(),
        qos,
        retain);

    if (msg_id >= 0) {
        ESP_LOGI(TAG, "PUBLISH OK: topic=%s msg_id=%d len=%d", topic.c_str(), msg_id, (int)json.size());
        ESP_LOGI(TAG, "PUBLISH payload: %s", json.c_str());
    } else {
        ESP_LOGE(TAG, "PUBLISH FAIL: topic=%s err=%d", topic.c_str(), msg_id);
    }

    return msg_id >= 0;
}

bool MqttClient::subscribe(const std::string& topic, int qos)
{
    if (!client_ || !connected_) {
        ESP_LOGW(TAG, "SUBSCRIBE FAIL (not connected): topic=%s", topic.c_str());
        return false;
    }

    int msg_id = esp_mqtt_client_subscribe(
        client_,
        topic.c_str(),
        qos);

    if (msg_id >= 0) {
        ESP_LOGI(TAG, "SUBSCRIBE OK: topic=%s qos=%d msg_id=%d", topic.c_str(), qos, msg_id);
    } else {
        ESP_LOGE(TAG, "SUBSCRIBE FAIL: topic=%s err=%d", topic.c_str(), msg_id);
    }

    return msg_id >= 0;
}

void MqttClient::onConnected(std::function<void()> cb)
{
    connected_cb_ = cb;
}

void MqttClient::onDisconnected(std::function<void()> cb)
{
    disconnected_cb_ = cb;
}

void MqttClient::onMessage(
    std::function<void(const std::string&, const std::string&)> cb)
{
    message_cb_ = cb;
}

bool MqttClient::isConnected() const
{
    return connected_;
}

// ------------------------------------------------------------------
// Event handling
// ------------------------------------------------------------------

void MqttClient::eventHandlerStatic(void* handler_args,
                                   esp_event_base_t,
                                   int32_t,
                                   void* event_data)
{
    auto* self = static_cast<MqttClient*>(handler_args);
    auto* event = static_cast<esp_mqtt_event_handle_t>(event_data);
    self->eventHandler(event);
}

void MqttClient::eventHandler(esp_mqtt_event_handle_t event)
{
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "EVENT: CONNECTED session_present=%d", event->session_present);
        connected_ = true;
        if (connected_cb_)
            connected_cb_();
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "EVENT: DISCONNECTED");
        connected_ = false;
        if (disconnected_cb_)
            disconnected_cb_();
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "EVENT: SUBACK msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "EVENT: UNSUBACK msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "EVENT: PUBACK msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "EVENT: DATA topic_len=%d data_len=%d msg_id=%d",
                 event->topic_len, event->data_len, event->msg_id);
        if (message_cb_)
        {
            std::string topic;
            std::string payload;

            if (event->topic && event->topic_len > 0)
                topic.assign(event->topic, event->topic_len);

            if (event->data && event->data_len > 0)
                payload.assign(event->data, event->data_len);

            message_cb_(topic, payload);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "EVENT: ERROR type=%d", event->error_handle->error_type);
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "  esp_tls=%d tls_stack=%d transport_sock=%d",
                     event->error_handle->esp_tls_last_esp_err,
                     event->error_handle->esp_tls_stack_err,
                     event->error_handle->esp_transport_sock_errno);
        }
        break;

    default:
        ESP_LOGD(TAG, "EVENT: id=%d", event->event_id);
        break;
    }
}
