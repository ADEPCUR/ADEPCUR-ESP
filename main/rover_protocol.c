#include "rover_protocol.h"

uint8_t simpleprotocol_crc8(const uint8_t *data, uint32_t len) {
    uint8_t crc = 0xFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

static int frame_wrap(uint8_t *buf, uint8_t *payload, uint8_t payload_len) {
    buf[0] = 0x40; // SYNC0
    buf[1] = 0x3C; // SYNC1
    buf[2] = payload_len;
    for (int i = 0; i < payload_len; i++) {
        buf[3 + i] = payload[i];
    }
    buf[3 + payload_len] = simpleprotocol_crc8(payload, payload_len);
    buf[3 + payload_len + 1] = 0x0A; // TERMINATOR
    return 3 + payload_len + 2;
}

int simpleprotocol_encode_tc_arm(uint8_t *buf, bool arm) {
    uint8_t payload[2];
    payload[0] = TC_ARM;
    payload[1] = arm ? 1 : 0;
    return frame_wrap(buf, payload, 2);
}

int simpleprotocol_encode_tc_throttle(uint8_t *buf, int16_t left, int16_t right) {
    uint8_t payload[5];
    payload[0] = TC_SET_THROTTLE;
    payload[1] = left & 0xFF;
    payload[2] = (left >> 8) & 0xFF;
    payload[3] = right & 0xFF;
    payload[4] = (right >> 8) & 0xFF;
    return frame_wrap(buf, payload, 5);
}
