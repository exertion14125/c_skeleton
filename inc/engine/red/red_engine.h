#ifndef __ENGINE_RED_RED_ENGINE_H__
#define __ENGINE_RED_RED_ENGINE_H__

#include <stdint.h>

#include "mgr/red/red_policy.h"

typedef enum red_eng_input_kind_e {
        RED_ENG_IN_NONE = 0,
        RED_ENG_IN_EVAL_REQ = 1,
        RED_ENG_IN_PEER_OK = 2,
        RED_ENG_IN_PEER_ERR = 3,
        RED_ENG_IN_TIMEOUT = 4,
        RED_ENG_IN_OPERATOR_HOLD = 5,
        RED_ENG_IN_OPERATOR_RELEASE = 6,
        RED_ENG_IN_TICK_EVAL = 7
} red_eng_input_kind_t;

typedef enum red_eng_action_e {
        RED_ENG_ACT_NONE = 0,
        RED_ENG_ACT_KEEP = 1,
        RED_ENG_ACT_PROPOSE_FAILOVER = 2,
        RED_ENG_ACT_PROPOSE_RECOVER = 3,
        RED_ENG_ACT_PROPOSE_HOLD = 4,
        RED_ENG_ACT_PROPOSE_SAFE = 5
} red_eng_action_t;

typedef struct red_eng_input_s {
        red_eng_input_kind_t kind;
        uint32_t req_id;
        int32_t arg0;
        int32_t arg1;
        uint64_t now_ms;
} red_eng_input_t;

typedef struct red_eng_output_s {
        red_eng_action_t action;
        uint32_t req_id;
        uint32_t reason_code;
        uint32_t risk_flags;
        int32_t score;
        int32_t value;
} red_eng_output_t;

typedef struct red_engine_s red_engine_t;

red_engine_t *alloc_red_engine(void);
void destroy_red_engine(red_engine_t **pe);

int init_red_engine(red_engine_t *e);
void deinit_red_engine(red_engine_t *e);

int red_engine_apply(red_engine_t *e,
                     const red_eng_input_t *in,
                     red_eng_output_t *out);

#endif /* __ENGINE_RED_RED_ENGINE_H__ */