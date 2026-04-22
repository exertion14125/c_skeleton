#ifndef __ENGINE_LOGIC_LOGIC_OUTPUT_MAP_PRIV_H__
#define __ENGINE_LOGIC_LOGIC_OUTPUT_MAP_PRIV_H__

#include "engine/logic/logic_output_map.h"

struct logic_output_map_s {
        uint32_t route_count;
        logic_output_route_cfg_t routes[LOGIC_OUTPUT_ROUTE_MAX];
};
#endif /* __ENGINE_LOGIC_LOGIC_OUTPUT_MAP_PRIV_H__ */