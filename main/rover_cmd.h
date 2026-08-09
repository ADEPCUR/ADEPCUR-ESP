#ifndef ROVER_CMD_H
#define ROVER_CMD_H

#include <stdint.h>

/*
 * Commands passed from a control surface (WS handler today; BLE/etc. could
 * push the same queue later) to rover_control_task via cmd_queue.
 *
 * This used to be declared separately (and identically) in main.c and
 * web_server.c. Since cmd_queue is typed by sizeof(rover_cmd_msg_t), a
 * future edit to only one copy would silently desync producer and
 * consumer - sharing one definition removes that trap.
 */
typedef enum {
    CMD_STOP = 0,
    CMD_FWD,
    CMD_REV,
    CMD_LEFT,
    CMD_RIGHT,
    CMD_ARM,
    CMD_DISARM,
    CMD_THROTTLE, /* explicit analog left/right, see rover_cmd_msg_t.left/right */
} rover_cmd_t;

typedef struct {
    rover_cmd_t cmd;
    int16_t left;  /* only meaningful for CMD_THROTTLE, milli-units -1000..1000 */
    int16_t right; /* only meaningful for CMD_THROTTLE, milli-units -1000..1000 */
} rover_cmd_msg_t;

#endif
