#ifndef UTIL_IPC_SYSTEM_SHM_LAYOUT_H
#define UTIL_IPC_SYSTEM_SHM_LAYOUT_H

#include <stdint.h>
#include <stddef.h>

#include "util/ipc/shm_common.h"
#include "resource/cfg/cfg_shm_layout.h"
#include "resource/gio/gio_shm_layout.h"
#include "resource/red/red_shm_layout.h"

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

#endif /* UTIL_IPC_SYSTEM_SHM_LAYOUT_H */
