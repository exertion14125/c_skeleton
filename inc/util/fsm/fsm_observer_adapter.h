#ifndef __UTIL_FSM_OBSERVER_ADAPTER_H__
#define __UTIL_FSM_OBSERVER_ADAPTER_H__

#include <stdint.h>
#include "util/fsm/fsm.h"
#include "util/fsm/fsm_observer_if.h"

typedef struct fsm_observer_adapter_s fsm_observer_adapter_t;

extern fsm_observer_adapter_t* alloc_fsm_observer_adapter(void);
extern void destroy_fsm_observer_adapter(fsm_observer_adapter_t **p);

extern void fsm_observer_adapter_init_hook(fsm_hook_t *out_hook, fsm_observer_adapter_t *ctx, const fsm_observer_t *obs);

#endif /* __UTIL_FSM_OBSERVER_ADAPTER_H__ */