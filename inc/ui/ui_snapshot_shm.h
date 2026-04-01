#ifndef __UI_SNAPSHOT_SHM_H__
#define __UI_SNAPSHOT_SHM_H__

#include <stdint.h>

#define UI_SNAPSHOT_MAGIC   0x55495348u
#define UI_SNAPSHOT_VERSION 1u

typedef struct ui_snapshot_hdr_s {
        uint32_t magic;
        uint16_t version;
        uint16_t hdr_size;
        uint32_t total_size;

        uint32_t active_idx;
        uint32_t seq;
        uint32_t flags;
        uint32_t reserved;
} ui_snapshot_hdr_t;

typedef struct ui_snapshot_payload_s {
        uint32_t page_id;
        uint32_t conn_state;
        uint32_t alarm_count;
        uint32_t reserved0;

        char title[64];
        char status_line[128];
} ui_snapshot_payload_t;

typedef struct ui_snapshot_shm_layout_s {
        ui_snapshot_hdr_t hdr;
        ui_snapshot_payload_t buf[2];
} ui_snapshot_shm_layout_t;

#endif /* __UI_SNAPSHOT_SHM_H__ */