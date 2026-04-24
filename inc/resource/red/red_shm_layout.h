#ifndef RESOURCE_RED_RED_SHM_LAYOUT_H
#define RESOURCE_RED_RED_SHM_LAYOUT_H

#include <stdint.h>
#include "util/ipc/shm_common.h"

#define SHM_RED_PAYLOAD_MAX 512u

typedef struct shm_red_slot_s {
    shm_slot_hdr_t hdr;
    uint8_t payload[SHM_RED_PAYLOAD_MAX];
} shm_red_slot_t;

typedef struct red_seg_s {
    shm_dbuf_ctrl_t ctrl;
    shm_red_slot_t slots[2];
} red_seg_t;

#endif /* RESOURCE_RED_RED_SHM_LAYOUT_H */
