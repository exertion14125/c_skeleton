#ifndef UTIL_IPC_SHM_DBUF_H
#define UTIL_IPC_SHM_DBUF_H

#include <stdint.h>

#include "util/ipc/shm_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct shm_dbuf_view_s {
    shm_dbuf_ctrl_t *ctrl;
    shm_slot_hdr_t *slot_hdr[2];
    uint8_t *slot_payload[2];
    uint32_t payload_capacity;
} shm_dbuf_view_t;

int shm_dbuf_view_bind(
    shm_dbuf_view_t *view,
    shm_dbuf_ctrl_t *ctrl,
    shm_slot_hdr_t *slot0_hdr,
    uint8_t *slot0_payload,
    shm_slot_hdr_t *slot1_hdr,
    uint8_t *slot1_payload,
    uint32_t payload_capacity);

int shm_dbuf_ctrl_reset(shm_dbuf_ctrl_t *ctrl, uint32_t payload_size);

int shm_dbuf_publish(
    shm_dbuf_view_t *view,
    const void *payload,
    uint32_t payload_size,
    uint32_t slot_flags,
    uint32_t ctrl_flags);

int shm_dbuf_read_snapshot(
    const shm_dbuf_view_t *view,
    void *out_snapshot,
    uint32_t out_capacity,
    uint32_t *out_size,
    uint32_t *out_seq);

#ifdef __cplusplus
}
#endif

#endif /* UTIL_IPC_SHM_DBUF_H */
