#ifndef __UTIL_OBS_DISPATCH_OBSERVER_OBS_H__
#define __UTIL_OBS_DISPATCH_OBSERVER_OBS_H__

#include "util/dispatch/dispatch_observer_if.h"
#include "util/obs/obs.h"

struct dispatch_observer_obs_s;
typedef struct dispatch_observer_obs_s dispatch_observer_obs_t;

extern dispatch_observer_obs_t* alloc_dispatch_observer_obs(void);
extern void destroy_dispatch_observer_obs(dispatch_observer_obs_t **pctx);
extern void dispatch_observer_obs_init(dispatch_observer_t *out, dispatch_observer_obs_t *ctx, obs_t *obs, uint32_t emit_dispatch, uint32_t emit_drop, uint32_t emit_cancel, uint32_t emit_fsm);

#endif /* __UTIL_OBS_DISPATCH_OBSERVER_OBS_H__ */