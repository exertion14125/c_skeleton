#ifndef __RESOURCE_GIO_GIO_SNAPSHOT_DTO_H__
#define __RESOURCE_GIO_GIO_SNAPSHOT_DTO_H__

#include <stdint.h>

/* ============================================================
 *  CONSTANTS
 * ============================================================ */

#define GIO_MAX_CARDS             24U
#define GIO_MAX_IO_PER_CARD       24U

/* ============================================================
 *  CARD TYPE
 * ============================================================ */

typedef enum gio_card_type_e {
        GIO_CARD_TYPE_NONE = 0,
        GIO_CARD_TYPE_DI   = 1,
        GIO_CARD_TYPE_DO   = 2,
        GIO_CARD_TYPE_AI   = 3,
        GIO_CARD_TYPE_AO   = 4,
        GIO_CARD_TYPE_RED  = 5
} gio_card_type_t;

/* ============================================================
 *  CONTROL (SYNC / EPOCH / TIME)
 * ============================================================ */

typedef struct gio_shm_ctrl_s {
        uint32_t req_epoch;       /* GIO -> IO proc 요청 세대 */
        uint32_t rsp_epoch;       /* IO proc -> GIO 응답 세대 */
        uint32_t apply_epoch;     /* logic output 적용 세대 */

        uint32_t flags;           /* timeout/degraded/new-data 등 */

        uint64_t req_ts_ms;       /* 요청 시각 */
        uint64_t rsp_ts_ms;       /* 응답 완료 시각 */
        uint64_t apply_ts_ms;     /* output 반영 시각 */
} gio_shm_ctrl_t;

/* ============================================================
 *  RED INPUT (for redundancy manager)
 * ============================================================ */

typedef struct gio_red_input_s {
        uint32_t valid;
        uint32_t link_state;
        uint32_t peer_state;
        uint32_t alarm_flags;
        uint64_t ts_ms;
} gio_red_input_t;

/* ============================================================
 *  CARD INPUT SNAPSHOT
 * ============================================================ */

typedef struct gio_card_input_s {
        uint32_t valid;
        uint32_t card_no;
        uint32_t card_type;
        uint32_t channel_count;

        uint32_t err_flags;
        uint32_t err_count;

        uint64_t ts_ms;

        int32_t value[GIO_MAX_IO_PER_CARD];
} gio_card_input_t;

/* ============================================================
 *  GLOBAL INPUT SNAPSHOT
 * ============================================================ */

typedef struct gio_input_snapshot_s {
        uint32_t snap_epoch;
        uint32_t card_count;

        uint32_t global_err_flags;
        uint32_t global_err_count;

        uint64_t ts_ms;

        gio_red_input_t red_input;

        gio_card_input_t cards[GIO_MAX_CARDS];
} gio_input_snapshot_t;

/* ============================================================
 *  CARD OUTPUT SNAPSHOT
 * ============================================================ */

typedef struct gio_card_output_s {
        uint32_t valid;
        uint32_t card_no;
        uint32_t card_type;
        uint32_t channel_count;

        uint64_t ts_ms;

        int32_t value[GIO_MAX_IO_PER_CARD];
} gio_card_output_t;

/* ============================================================
 *  GLOBAL OUTPUT SNAPSHOT
 * ============================================================ */

typedef struct gio_output_snapshot_s {
        uint32_t apply_epoch;
        uint32_t card_count;
        uint64_t ts_ms;

        gio_card_output_t cards[GIO_MAX_CARDS];
} gio_output_snapshot_t;

/* ============================================================
 *  ROOT SHARED MEMORY STRUCTURE
 * ============================================================ */

typedef struct gio_shared_memory_s {
        gio_shm_ctrl_t        ctrl;
        gio_input_snapshot_t  input;
        gio_output_snapshot_t output;
} gio_shared_memory_t;

#endif /* __RESOURCE_GIO_GIO_SNAPSHOT_DTO_H__ */