#ifndef __ENGINE_LOGIC_LOGIC_OUTPUT_MAP_H__
#define __ENGINE_LOGIC_LOGIC_OUTPUT_MAP_H__

#include <stdint.h>

#include "engine/logic/logic_engine.h"
#include "resource/gio/gio_snapshot_dto.h"

#define LOGIC_OUTPUT_ROUTE_MAX   8U

typedef struct logic_output_route_cfg_s {
        uint32_t valid;
        uint32_t out_card_no;
        uint32_t out_card_type;
        uint32_t out_value_index;   /* 0 -> value0, 1 -> value1 */
        uint32_t out_ch;
} logic_output_route_cfg_t;

typedef struct logic_output_map_cfg_s {
        uint32_t route_count;
        logic_output_route_cfg_t routes[LOGIC_OUTPUT_ROUTE_MAX];
} logic_output_map_cfg_t;

typedef struct logic_output_map_s logic_output_map_t;

logic_output_map_t *alloc_logic_output_map(void);
void destroy_logic_output_map(logic_output_map_t **pm);

int init_logic_output_map(logic_output_map_t *m, const logic_output_map_cfg_t *cfg);
void deinit_logic_output_map(logic_output_map_t *m);

int logic_output_map_apply_cfg(logic_output_map_t *m,
                               const logic_output_map_cfg_t *cfg);

int logic_output_map_apply(logic_output_map_t *m,
                           const logic_eng_output_t *logic_out,
                           uint64_t now_ms,
                           gio_output_snapshot_t *out_snap);

#endif /* __ENGINE_LOGIC_LOGIC_OUTPUT_MAP_H__ */