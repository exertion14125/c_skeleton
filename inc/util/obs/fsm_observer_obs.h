#ifndef __UTIL_OBS_FSM_OBSERVER_OBS_H__
#define __UTIL_OBS_FSM_OBSERVER_OBS_H__

#include <stdint.h>

#include "util/obs/obs.h"
#include "util/fsm/fsm_observer_if.h"

typedef struct fsm_observer_obs_s fsm_observer_obs_t;

extern fsm_observer_obs_t* alloc_fsm_observer_obs(void);
extern void destroy_fsm_observer_obs(fsm_observer_obs_t **pctx);
extern void fsm_observer_obs_init(fsm_observer_t *out, fsm_observer_obs_t *ctx, obs_t *obs, uint32_t emit_step, uint32_t emit_enter_exit, uint32_t emit_noop, uint32_t emit_guard_fail, uint32_t emit_transition);

#endif /* __UTIL_OBS_FSM_OBSERVER_OBS_H__ */