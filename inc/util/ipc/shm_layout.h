#ifndef UTIL_IPC_SHM_LAYOUT_H
#define UTIL_IPC_SHM_LAYOUT_H

#include <stdint.h>
#include <stddef.h>

#include "util/ipc/shm_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHM_CFG_PAYLOAD_MAX      512u
#define SHM_GIO_IN_PAYLOAD_MAX   4096u
#define SHM_GIO_OUT_PAYLOAD_MAX  4096u
#define SHM_RED_PAYLOAD_MAX      512u

typedef struct shm_cfg_slot_s {
    shm_slot_hdr_t hdr;
    uint8_t payload[SHM_CFG_PAYLOAD_MAX];
} shm_cfg_slot_t;

typedef struct shm_gio_in_slot_s {
    shm_slot_hdr_t hdr;
    uint8_t payload[SHM_GIO_IN_PAYLOAD_MAX];
} shm_gio_in_slot_t;

typedef struct shm_gio_out_slot_s {
    shm_slot_hdr_t hdr;
    uint8_t payload[SHM_GIO_OUT_PAYLOAD_MAX];
} shm_gio_out_slot_t;

typedef struct shm_red_slot_s {
    shm_slot_hdr_t hdr;
    uint8_t payload[SHM_RED_PAYLOAD_MAX];
} shm_red_slot_t;

typedef struct cfg_seg_s {
    shm_dbuf_ctrl_t ctrl;
    shm_cfg_slot_t slots[2];
} cfg_seg_t;

typedef struct gio_in_seg_s {
    shm_dbuf_ctrl_t ctrl;
    shm_gio_in_slot_t slots[2];
} gio_in_seg_t;

typedef struct gio_out_seg_s {
    shm_dbuf_ctrl_t ctrl;
    shm_gio_out_slot_t slots[2];
} gio_out_seg_t;

typedef struct red_seg_s {
    shm_dbuf_ctrl_t ctrl;
    shm_red_slot_t slots[2];
} red_seg_t;

typedef struct system_shm_s {
    shm_global_hdr_t hdr;
    cfg_seg_t cfg;
    gio_in_seg_t gio_in;
    gio_out_seg_t gio_out;
    red_seg_t red;
} system_shm_t;

#define SHM_CT_ASSERT(name, expr) typedef char shm_ct_assert_##name[(expr) ? 1 : -1]

SHM_CT_ASSERT(global_hdr_size_match, sizeof(shm_global_hdr_t) == 28u);
SHM_CT_ASSERT(dbuf_ctrl_size_match, sizeof(shm_dbuf_ctrl_t) == 16u);
SHM_CT_ASSERT(slot_hdr_size_match, sizeof(shm_slot_hdr_t) == 16u);

SHM_CT_ASSERT(cfg_segment_after_hdr, offsetof(system_shm_t, cfg) >= sizeof(shm_global_hdr_t));
SHM_CT_ASSERT(gio_in_after_cfg, offsetof(system_shm_t, gio_in) > offsetof(system_shm_t, cfg));
SHM_CT_ASSERT(gio_out_after_gio_in, offsetof(system_shm_t, gio_out) > offsetof(system_shm_t, gio_in));
SHM_CT_ASSERT(red_after_gio_out, offsetof(system_shm_t, red) > offsetof(system_shm_t, gio_out));

#ifdef __cplusplus
}
#endif

#endif /* UTIL_IPC_SHM_LAYOUT_H */
