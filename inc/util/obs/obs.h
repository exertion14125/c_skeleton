#ifndef __UTIL_OBS_H__
#define __UTIL_OBS_H__

#include <stdint.h>
#include "util/obs/obs_types.h"

typedef struct obs_s obs_t;

typedef uint64_t (*obs_now_ms_fn)(void *user);

extern obs_t* alloc_obs(void);
extern void destroy_obs(obs_t **po);

extern int obs_init(obs_t *o, obs_evt_t *ring_mem, uint32_t ring_cap, obs_now_ms_fn now_fn, void *now_user);

extern void obs_epoch_inc(obs_t *o);
extern uint64_t obs_epoch_get(const obs_t *o);

extern int obs_push(obs_t *o, uint32_t type, int32_t a, int32_t b, int32_t c); /// return: 1 pushed, 0 dropped(full), <0 err

extern int obs_pop(obs_t *o, obs_evt_t *out); /// return: 1 got, 0 empty, <0 err

extern void obs_get_snapshot(const obs_t *o, obs_snapshot_t *out);

extern void obs_get_kpi(const obs_t *o, obs_kpi_t *out);
#endif