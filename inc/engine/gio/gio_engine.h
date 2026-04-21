#ifndef __ENGINE_GIO_GIO_ENGINE_H__
#define __ENGINE_GIO_GIO_ENGINE_H__

#include <stdint.h>

#include "resource/gio/gio_snapshot_dto.h"

typedef enum gio_eng_input_kind_e {
        GIO_ENG_IN_NONE = 0,
        GIO_ENG_IN_EXEC_RESULT = 1,
        GIO_ENG_IN_TICK = 2
} gio_eng_input_kind_t;

typedef enum gio_eng_action_e {
        GIO_ENG_ACT_NONE = 0,
        GIO_ENG_ACT_EXEC_DONE = 1,
        GIO_ENG_ACT_TIMEOUT_EVT = 2,
        GIO_ENG_ACT_DEGRADED_EVT = 3,
        GIO_ENG_ACT_KEEP = 4
} gio_eng_action_t;

typedef struct gio_eng_input_s {
        gio_eng_input_kind_t kind;
        uint32_t req_id;
        int32_t raw_rc;
        int32_t arg0;
        uint64_t now_ms;
} gio_eng_input_t;

typedef struct gio_eng_output_s {
        gio_eng_action_t action;
        uint32_t req_id;
        int32_t rc;
        int32_t value0;
} gio_eng_output_t;

typedef struct gio_engine_s gio_engine_t;

gio_engine_t *alloc_gio_engine(void);
void destroy_gio_engine(gio_engine_t **pe);

int init_gio_engine(gio_engine_t *e);
void deinit_gio_engine(gio_engine_t *e);

int gio_engine_apply(gio_engine_t *e,
                     const gio_eng_input_t *in,
                     gio_eng_output_t *out);

int gio_engine_apply_snapshot(gio_engine_t *e,
                              const gio_shm_ctrl_t *ctrl,
                              const gio_input_snapshot_t *input,
                              uint64_t now_ms,
                              gio_eng_output_t *out);

#endif /* __ENGINE_GIO_GIO_ENGINE_H__ */