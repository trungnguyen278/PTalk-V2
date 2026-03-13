#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

// =============================================================================
// UART Protocol between ESP32-C5 and ESP32-S3
// Frame format: [0xAA] [0x55] [TYPE:1] [LEN:2 LE] [PAYLOAD:LEN] [CRC8:1]
// =============================================================================

namespace uart_proto {

// Frame markers
static constexpr uint8_t FRAME_HEADER_0 = 0xAA;
static constexpr uint8_t FRAME_HEADER_1 = 0x55;
static constexpr size_t  FRAME_OVERHEAD = 6; // 2 header + 1 type + 2 len + 1 crc
static constexpr size_t  MAX_PAYLOAD    = 256;

// --- Message types: S3 -> C5 ---
enum class MsgFromS3 : uint8_t {
    WIFI_CONFIG   = 0x01, // {ssid_len:1, ssid:N, pass_len:1, pass:N}
    MQTT_CONFIG   = 0x02, // {uri_len:1, uri:N, user_len:1, user:N, pass_len:1, pass:N}
    WS_CONFIG     = 0x03, // {url_len:1, url:N}
    ADC_DATA      = 0x04, // {voltage_mv:2LE, battery_pct:1, charging:1}
    COMMAND       = 0x05, // {cmd:1, data:N} (reboot, set_volume, etc.)
    ACK           = 0x06, // {ref_type:1, status:1}
};

// --- Message types: C5 -> S3 ---
enum class MsgToS3 : uint8_t {
    STATUS_UPDATE = 0x81, // {interaction:1, connectivity:1, system:1}
    DISPLAY_CMD   = 0x82, // {emotion:1}
    CONFIG_REQ    = 0x83, // {} (request S3 to send saved configs)
    ACK           = 0x84, // {ref_type:1, status:1}
};

// Command sub-types for COMMAND message
enum class Command : uint8_t {
    REBOOT        = 0x01,
    SET_VOLUME    = 0x02, // +1 byte volume (0-100)
    FACTORY_RESET = 0x03,
};

// ACK status
enum class AckStatus : uint8_t {
    OK   = 0x00,
    FAIL = 0x01,
};

// CRC-8 (simple XOR)
inline uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
    }
    return crc;
}

// Build a frame into `out` buffer. Returns total frame size, or 0 on error.
inline size_t buildFrame(uint8_t* out, size_t out_capacity,
                         uint8_t msg_type, const uint8_t* payload, uint16_t payload_len)
{
    size_t total = FRAME_OVERHEAD + payload_len;
    if (total > out_capacity || payload_len > MAX_PAYLOAD) return 0;

    out[0] = FRAME_HEADER_0;
    out[1] = FRAME_HEADER_1;
    out[2] = msg_type;
    out[3] = (uint8_t)(payload_len & 0xFF);
    out[4] = (uint8_t)((payload_len >> 8) & 0xFF);
    if (payload_len > 0 && payload) {
        memcpy(&out[5], payload, payload_len);
    }
    // CRC over type + len + payload
    out[5 + payload_len] = crc8(&out[2], 3 + payload_len);
    return total;
}

// Frame parser state machine
class FrameParser {
public:
    enum class Result { NEED_MORE, FRAME_READY, ERROR };

    void reset() { state_ = State::WAIT_H0; idx_ = 0; }

    Result feed(uint8_t byte) {
        switch (state_) {
        case State::WAIT_H0:
            if (byte == FRAME_HEADER_0) state_ = State::WAIT_H1;
            return Result::NEED_MORE;
        case State::WAIT_H1:
            if (byte == FRAME_HEADER_1) { state_ = State::WAIT_TYPE; }
            else { state_ = State::WAIT_H0; }
            return Result::NEED_MORE;
        case State::WAIT_TYPE:
            msg_type_ = byte;
            state_ = State::WAIT_LEN0;
            return Result::NEED_MORE;
        case State::WAIT_LEN0:
            payload_len_ = byte;
            state_ = State::WAIT_LEN1;
            return Result::NEED_MORE;
        case State::WAIT_LEN1:
            payload_len_ |= ((uint16_t)byte << 8);
            if (payload_len_ > MAX_PAYLOAD) { reset(); return Result::ERROR; }
            idx_ = 0;
            state_ = (payload_len_ > 0) ? State::PAYLOAD : State::WAIT_CRC;
            return Result::NEED_MORE;
        case State::PAYLOAD:
            payload_[idx_++] = byte;
            if (idx_ >= payload_len_) state_ = State::WAIT_CRC;
            return Result::NEED_MORE;
        case State::WAIT_CRC: {
            // Verify CRC: type + len_lo + len_hi + payload
            uint8_t check_buf[3 + MAX_PAYLOAD];
            check_buf[0] = msg_type_;
            check_buf[1] = (uint8_t)(payload_len_ & 0xFF);
            check_buf[2] = (uint8_t)((payload_len_ >> 8) & 0xFF);
            if (payload_len_ > 0) memcpy(&check_buf[3], payload_, payload_len_);
            uint8_t expected = crc8(check_buf, 3 + payload_len_);
            state_ = State::WAIT_H0;
            if (byte == expected) return Result::FRAME_READY;
            return Result::ERROR;
        }
        }
        return Result::ERROR;
    }

    uint8_t        msgType()    const { return msg_type_; }
    const uint8_t* payload()    const { return payload_; }
    uint16_t       payloadLen() const { return payload_len_; }

private:
    enum class State { WAIT_H0, WAIT_H1, WAIT_TYPE, WAIT_LEN0, WAIT_LEN1, PAYLOAD, WAIT_CRC };
    State    state_ = State::WAIT_H0;
    uint8_t  msg_type_ = 0;
    uint16_t payload_len_ = 0;
    uint16_t idx_ = 0;
    uint8_t  payload_[MAX_PAYLOAD] = {};
};

} // namespace uart_proto
