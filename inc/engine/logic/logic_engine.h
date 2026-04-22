#ifndef __ENGINE_LOGIC_LOGIC_ENGINE_H__
#define __ENGINE_LOGIC_LOGIC_ENGINE_H__

#include <stdint.h>

#include "resource/gio/gio_snapshot_dto.h"

#define LOGIC_OUTPUT_VALUE_MAX   8U

typedef enum logic_eng_input_kind_e {
        LOGIC_ENG_IN_NONE = 0,
        LOGIC_ENG_IN_EXEC_REQ = 1,
        LOGIC_ENG_IN_TICK = 2
} logic_eng_input_kind_t;

typedef enum logic_eng_action_e {
        LOGIC_ENG_ACT_NONE = 0,
        LOGIC_ENG_ACT_EXEC_DONE = 1,
        LOGIC_ENG_ACT_KEEP = 2
} logic_eng_action_t;

typedef struct logic_eng_input_s {
        logic_eng_input_kind_t kind;
        uint32_t req_id;
        int32_t arg0;
        int32_t arg1;
        uint64_t now_ms;
} logic_eng_input_t;

typedef struct logic_eng_output_s {
        logic_eng_action_t action;
        uint32_t req_id;
        int32_t rc;

        uint32_t value_count;
        int32_t values[LOGIC_OUTPUT_VALUE_MAX];
} logic_eng_output_t;

typedef struct logic_engine_s logic_engine_t;

logic_engine_t *alloc_logic_engine(void);
void destroy_logic_engine(logic_engine_t **pe);

int init_logic_engine(logic_engine_t *e);
void deinit_logic_engine(logic_engine_t *e);

int logic_engine_apply(logic_engine_t *e,
                       const logic_eng_input_t *in,
                       logic_eng_output_t *out);

int logic_engine_apply_snapshot(logic_engine_t *e,
                                const gio_input_snapshot_t *in,
                                uint64_t now_ms,
                                logic_eng_output_t *out);

#endif /* __ENGINE_LOGIC_LOGIC_ENGINE_H__ */