#ifndef ROVER_PROTOCOL_H
#define ROVER_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define TC_LED            0x01
#define TC_ARM            0x02
#define TC_SET_THROTTLE   0x03

uint8_t simpleprotocol_crc8(const uint8_t *data, uint32_t len);
int simpleprotocol_encode_tc_arm(uint8_t *buf, bool arm);
int simpleprotocol_encode_tc_throttle(uint8_t *buf, int16_t left, int16_t right);

#endif
