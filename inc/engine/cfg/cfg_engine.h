#ifndef __ENGINE_CFG_CFG_ENGINE_H__
#define __ENGINE_CFG_CFG_ENGINE_H__

#include <stdint.h>

#include "resource/cfg/dto/cfg_request_dto.h"

typedef enum cfg_eng_input_kind_e {
        CFG_ENG_IN_NONE = 0,
        CFG_ENG_IN_OPEN_REQ = 1,
        CFG_ENG_IN_ADJUST_REQ = 2,
        CFG_ENG_IN_REOPEN_REQ = 3,
        CFG_ENG_IN_MODIFY_REQ = 4,
        CFG_ENG_IN_TICK = 5
} cfg_eng_input_kind_t;

typedef enum cfg_eng_action_e {
        CFG_ENG_ACT_NONE = 0,
        CFG_ENG_ACT_OPEN_DONE = 1,
        CFG_ENG_ACT_ADJUST_DONE = 2,
        CFG_ENG_ACT_REOPEN_DONE = 3,
        CFG_ENG_ACT_MODIFY_DONE = 4,
        CFG_ENG_ACT_KEEP = 5
} cfg_eng_action_t;

typedef struct cfg_eng_input_s {
        cfg_eng_input_kind_t kind;
        uint32_t req_id;
        int32_t arg0;
        int32_t arg1;
        uint64_t now_ms;
} cfg_eng_input_t;

typedef struct cfg_eng_output_s {
        cfg_eng_action_t action;
        uint32_t req_id;
        int32_t rc;
        int32_t value0;
        int32_t value1;

        uint32_t logic_map_valid;
        uint32_t logic_out_card_no;
        uint32_t logic_out_card_type;
        uint32_t logic_out_ch0;
        uint32_t logic_out_ch1;
} cfg_eng_output_t;

typedef struct cfg_engine_s cfg_engine_t;

cfg_engine_t *alloc_cfg_engine(void);
void destroy_cfg_engine(cfg_engine_t **pe);

int init_cfg_engine(cfg_engine_t *e);
void deinit_cfg_engine(cfg_engine_t *e);

int cfg_engine_apply(cfg_engine_t *e,
                     const cfg_eng_input_t *in,
                     cfg_eng_output_t *out);

int cfg_engine_apply_request_dto(cfg_engine_t *e,
                                 const cfg_request_dto_t *req,
                                 cfg_eng_output_t *out);

#endif /* __ENGINE_CFG_CFG_ENGINE_H__ */