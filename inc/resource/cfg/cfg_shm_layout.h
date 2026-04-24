#ifndef RESOURCE_CFG_CFG_SHM_LAYOUT_H
#define RESOURCE_CFG_CFG_SHM_LAYOUT_H

#include <stdint.h>
#include "util/ipc/shm_common.h"

#define SHM_CFG_PAYLOAD_MAX 512u

typedef struct shm_cfg_slot_s {
    shm_slot_hdr_t hdr;
    uint8_t payload[SHM_CFG_PAYLOAD_MAX];
} shm_cfg_slot_t;

typedef struct cfg_seg_s {
    shm_dbuf_ctrl_t ctrl;
    shm_cfg_slot_t slots[2];
} cfg_seg_t;

#endif /* RESOURCE_CFG_CFG_SHM_LAYOUT_H */
