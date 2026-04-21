#ifndef __ENGINE_LOGIC_LOGIC_OUTPUT_MAP_H__
#define __ENGINE_LOGIC_LOGIC_OUTPUT_MAP_H__

#include <stdint.h>

#include "engine/logic/logic_engine.h"
#include "resource/gio/gio_snapshot_dto.h"

typedef struct logic_output_map_s logic_output_map_t;

logic_output_map_t *alloc_logic_output_map(void);
void destroy_logic_output_map(logic_output_map_t **pm);

int init_logic_output_map(logic_output_map_t *m);
void deinit_logic_output_map(logic_output_map_t *m);

int logic_output_map_apply(logic_output_map_t *m,
                           const logic_eng_output_t *logic_out,
                           uint64_t now_ms,
                           gio_output_snapshot_t *out_snap);

#endif /* __ENGINE_LOGIC_LOGIC_OUTPUT_MAP_H__ */