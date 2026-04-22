#ifndef __RESOURCE_CFG_CFG_RESULT_DTO_H__
#define __RESOURCE_CFG_CFG_RESULT_DTO_H__

#include <stdint.h>

typedef struct cfg_logic_map_result_s {
        uint32_t out_card_no;
        uint32_t out_card_type;
        uint32_t out_ch0;
        uint32_t out_ch1;
} cfg_logic_map_result_t;

typedef struct cfg_result_dto_s {
        uint32_t version;
        uint32_t valid;
        uint64_t ts_ms;

        cfg_logic_map_result_t logic_map;
} cfg_result_dto_t;

#endif /* __RESOURCE_CFG_CFG_RESULT_DTO_H__ */