#ifndef RESOURCE_GIO_GIO_SHM_LAYOUT_H
#define RESOURCE_GIO_GIO_SHM_LAYOUT_H

#include <stdint.h>
#include "util/ipc/shm_common.h"

#define SHM_GIO_IN_PAYLOAD_MAX   4096u
#define SHM_GIO_OUT_PAYLOAD_MAX  4096u

typedef struct shm_gio_in_slot_s {
    shm_slot_hdr_t hdr;
    uint8_t payload[SHM_GIO_IN_PAYLOAD_MAX];
} shm_gio_in_slot_t;

typedef struct shm_gio_out_slot_s {
    shm_slot_hdr_t hdr;
    uint8_t payload[SHM_GIO_OUT_PAYLOAD_MAX];
} shm_gio_out_slot_t;

typedef struct gio_in_seg_s {
    shm_dbuf_ctrl_t ctrl;
    shm_gio_in_slot_t slots[2];
} gio_in_seg_t;

typedef struct gio_out_seg_s {
    shm_dbuf_ctrl_t ctrl;
    shm_gio_out_slot_t slots[2];
} gio_out_seg_t;

#endif /* RESOURCE_GIO_GIO_SHM_LAYOUT_H */
