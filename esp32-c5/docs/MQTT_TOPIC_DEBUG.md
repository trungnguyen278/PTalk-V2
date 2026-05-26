# MQTT Topic Debug Report

**Device**: `D0CF13E35740`
**Broker**: `mqtt://171.226.10.121:8443`
**Date**: 2026-05-22

---

## 1. Log Analysis

```
I (60759) MqttClient: MQTT started: mqtt://171.226.10.121:8443
I (60819) MqttClient: MQTT connected
I (60819) NetworkManager: MQTT connected
I (65639) NetworkManager: MQTT: topic=devices/D0CF13E35740/cmd
I (75649) NetworkManager: MQTT: topic=devices/D0CF13E35740/cmd
```

**OK**: MQTT connect + subscribe thanh cong. Device nhan duoc 2 message tren topic `devices/D0CF13E35740/cmd`.

**Van de**: App khong nhan duoc topic phan hoi tu device.

---

## 2. Root Cause: Firmware chi publish response cho `request_status`

### 2.1 Firmware thuc te chi handle 3 commands (NetworkManager.cpp:208-239)

| Command | Xu ly | Publish response? | Topic response |
|---------|-------|-------------------|----------------|
| `set_volume` | Log only (chua relay S3) | **KHONG** | N/A |
| `reboot` | `esp_restart()` ngay | **KHONG** | N/A |
| `request_status` | Publish status | **CO** | `devices/{ID}/status` |
| (bat ky cmd khac) | Silent drop | **KHONG** | N/A |

**Ket luan**: Neu app gui `set_volume` hoac bat ky command nao khac `request_status`, device **se khong bao gio publish response**.

### 2.2 Docs mo ta MEO SDK protocol nhung firmware chua implement

Docs (`MQTT_SPEC.md`, `MQTT_SERVER_DEV.md`) mo ta 2 protocol:

| Protocol | Subscribe topic | Response topic | Firmware status |
|----------|-----------------|----------------|-----------------|
| **Legacy** | `devices/{MAC}/cmd` | `devices/{MAC}/status` | **Chi subscribe, chi response cho request_status** |
| **MEO SDK** | `meo/{userId}/{deviceId}/feature` | `meo/{userId}/{deviceId}/event/feature_response` | **CHUA IMPLEMENT** |

---

## 3. Cac nguyen nhan cu the

### Nguyen nhan 1 (Rat co the): App subscribe sai topic

**Neu app subscribe** `meo/{userId}/{deviceId}/event/feature_response`:
- Device **khong bao gio publish** len topic nay
- Firmware khong co MEO SDK feature handler

**Neu app subscribe** `devices/D0CF13E35740/status`:
- Device **chi publish** khi nhan command `request_status`
- Cac command khac (set_volume, reboot, ...) khong co response

### Nguyen nhan 2: App gui command nhung device khong response

Nhin log, device nhan 2 message tren `/cmd`. Nhung vi firmware chi log topic (khong log payload), khong biet chinh xac command gi duoc gui. Neu la `set_volume` -> khong co response.

### Nguyen nhan 3: MQTT broker ACL chan publish

Firmware publish len `devices/D0CF13E35740/status` nhung broker co the chua cau hinh ACL cho phep device write topic nay. Kiem tra broker ACL:
```
user D0CF13E35740
  topic write devices/D0CF13E35740/status
```

### Nguyen nhan 4: QoS 0 va network drop

Firmware publish voi QoS 0 (at-most-once) -> message co the mat ma khong co retry. Xem `MqttClient::publish()` default qos=0.

---

## 4. Debug Steps

### Step 1: Xac dinh payload cua cmd message

Them log payload vao firmware:
```cpp
// NetworkManager.cpp line 209
ESP_LOGI(TAG, "MQTT: topic=%s payload=%s", topic.c_str(), payload.c_str());
```

### Step 2: Test voi mosquitto CLI

```bash
# Terminal 1: Subscribe status topic de xem device co publish khong
mosquitto_sub -h 171.226.10.121 -p 8443 \
  -t "devices/D0CF13E35740/status" -v

# Terminal 2: Gui request_status (command duy nhat co response)
mosquitto_pub -h 171.226.10.121 -p 8443 \
  -t "devices/D0CF13E35740/cmd" \
  -m '{"cmd":"request_status"}'
```

### Step 3: Kiem tra app subscribe topic nao

Xac nhan app subscribe dung:
- **Legacy mode**: `devices/D0CF13E35740/status`
- **MEO SDK mode**: `meo/+/D0CF13E35740/event/#`

### Step 4: Kiem tra broker ACL

Dang nhap MQTT broker, kiem tra:
- Device co quyen publish len `devices/{ID}/status` khong
- App co quyen subscribe topic response khong

---

## 5. Topic Map: App <-> Device

```
APP (Publisher)                          DEVICE (Subscriber)
                                         
  devices/{ID}/cmd  ───────────────────>  devices/{ID}/cmd
  {"cmd":"request_status"}                  (subscribe on connect)
                                         
                                         
DEVICE (Publisher)                       APP (Subscriber)
                                         
  devices/{ID}/status  ────────────────>  devices/{ID}/status
  {"status":"ok",...}                       (app phai subscribe topic nay)
```

**Luu y**: Chi `request_status` command moi trigger device publish. Moi command khac deu **khong co response**.

---

## 6. Recommendations

### Fix 1 (Firmware - Uu tien cao): Them response cho moi command

```cpp
// Sau moi command handler, publish ack len /status hoac /response
if (cmd == "set_volume") {
    // ... handle volume ...
    std::string resp_topic = "devices/" + std::string(getDeviceEfuseID()) + "/response";
    char resp[128];
    snprintf(resp, sizeof(resp),
        "{\"cmd\":\"%s\",\"status\":\"ok\",\"device_id\":\"%s\"}",
        cmd.c_str(), getDeviceEfuseID());
    mqtt_->publish(resp_topic, resp);
}
```

### Fix 2 (Firmware): Log payload de debug

```cpp
ESP_LOGI(TAG, "MQTT: topic=%s payload=%.*s",
    topic.c_str(), (int)payload.size(), payload.c_str());
```

### Fix 3 (App): Dam bao subscribe dung topic

App phai subscribe `devices/{DEVICE_ID}/status` (Legacy mode) hoac doi firmware implement MEO SDK protocol.

### Fix 4 (Firmware): Nang QoS len 1

```cpp
mqtt_->publish(status_topic, buf, 1);  // QoS 1 = at-least-once
```

### Fix 5 (Long-term): Implement MEO SDK protocol

Firmware hien chi dung Legacy topics. Can implement full MEO SDK protocol voi:
- Subscribe: `meo/{userId}/{deviceId}/feature`
- Response: `meo/{userId}/{deviceId}/event/feature_response`
- Events: `meo/{userId}/{deviceId}/event/{eventName}`

---

## 7. Summary

| Check | Status |
|-------|--------|
| MQTT connect | OK |
| Subscribe `devices/{ID}/cmd` | OK |
| Nhan message tu server | OK (2 messages) |
| Publish response `devices/{ID}/status` | **Chi cho request_status** |
| MEO SDK feature handler | **CHUA IMPLEMENT** |
| Response cho set_volume | **KHONG CO** |
| Response cho reboot | **KHONG CO** |
| Log payload | **KHONG CO (chi log topic)** |
| QoS | 0 (co the mat message) |
