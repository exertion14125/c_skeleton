#ifndef __RESOURCE_LOGIC_LOGIC_CFG_DTO_H__
#define __RESOURCE_LOGIC_LOGIC_CFG_DTO_H__

#include <stdint.h>

#include "engine/logic/logic_output_map.h"

typedef struct logic_cfg_dto_s {
        uint32_t version;
        uint32_t valid;
        uint64_t ts_ms;

        logic_output_map_cfg_t out_map_cfg;
} logic_cfg_dto_t;

#endif /* __RESOURCE_LOGIC_LOGIC_CFG_DTO_H__ */