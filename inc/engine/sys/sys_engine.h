#ifndef __ENGINE_SYS_SYS_ENGINE_H__
#define __ENGINE_SYS_SYS_ENGINE_H__

#include <stdint.h>

typedef enum sys_eng_input_kind_e {
        SYS_ENG_IN_NONE = 0,
        SYS_ENG_IN_BOOTSTRAP = 1,
        SYS_ENG_IN_CFG_RSP = 2,
        SYS_ENG_IN_GIO_RSP = 3,
        SYS_ENG_IN_RED_RSP = 4,
        SYS_ENG_IN_RED_PROPOSE_FAILOVER = 5,
        SYS_ENG_IN_RED_PROPOSE_RECOVER = 6,
        SYS_ENG_IN_RED_PROPOSE_HOLD = 7,
        SYS_ENG_IN_GIO_RX_DONE = 8,
        SYS_ENG_IN_TICK = 9,
        SYS_ENG_IN_ERROR = 10,
        SYS_ENG_IN_LOGIC_RSP = 11
} sys_eng_input_kind_t;

typedef enum sys_eng_action_e {
        SYS_ENG_ACT_NONE = 0,
        SYS_ENG_ACT_KEEP = 1,

        SYS_ENG_ACT_SEND_CFG_OPEN = 2,
        SYS_ENG_ACT_SEND_CFG_ADJUST = 3,
        SYS_ENG_ACT_SEND_GIO_EXEC = 4,
        SYS_ENG_ACT_SEND_RED_EVAL = 5,

        SYS_ENG_ACT_CFG_ACCEPTED = 6,
        SYS_ENG_ACT_GIO_ACCEPTED = 7,
        SYS_ENG_ACT_RED_ACCEPTED = 8,

        SYS_ENG_ACT_APPROVE_FAILOVER = 9,
        SYS_ENG_ACT_REJECT_FAILOVER = 10,
        SYS_ENG_ACT_APPROVE_RECOVER = 11,
        SYS_ENG_ACT_REJECT_RECOVER = 12,
        SYS_ENG_ACT_ENTER_HOLD = 13,

        SYS_ENG_ACT_LOGIC_READY = 14,
        SYS_ENG_ACT_ENTER_ERR = 15
} sys_eng_action_t;

typedef struct sys_eng_input_s {
        sys_eng_input_kind_t kind;
        uint16_t msg_code;
        uint32_t req_id;
        int32_t arg0;
        int32_t arg1;
        uint64_t now_ms;
} sys_eng_input_t;

typedef struct sys_eng_output_s {
        sys_eng_action_t action;
        uint32_t req_id;
        int32_t value0;
        int32_t value1;
        uint32_t reason_code;
} sys_eng_output_t;

typedef struct sys_engine_s sys_engine_t;

sys_engine_t *alloc_sys_engine(void);
void destroy_sys_engine(sys_engine_t **pe);

int init_sys_engine(sys_engine_t *e);
void deinit_sys_engine(sys_engine_t *e);

int sys_engine_apply(sys_engine_t *e,
                     const sys_eng_input_t *in,
                     sys_eng_output_t *out);

#endif /* __ENGINE_SYS_SYS_ENGINE_H__ */