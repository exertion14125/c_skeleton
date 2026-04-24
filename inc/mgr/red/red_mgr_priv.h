#ifndef __MGR_RED_RED_MGR_PRIV_H__
#define __MGR_RED_RED_MGR_PRIV_H__

#include <stdint.h>
#include <pthread.h>

#include "mgr/red/red_mgr.h"
#include "engine/red/red_engine.h"
#include "ra/red/red_shm_ra.h"
#include "util/mgr_bus/mgr_bus.h"
#include "util/fsm/fsm.h"
#include "util/dispatch/dispatch.h"
#include "util/obs/obs.h"

typedef enum red_mgr_event_e {
        RED_MGR_EV_NONE = 0,
        RED_MGR_EV_START = 1,
        RED_MGR_EV_TICK = 2,
        RED_MGR_EV_TIMEOUT = 3,
        RED_MGR_EV_ERROR = 4,
        RED_MGR_EV_RECOVER = 5,
        RED_MGR_EV_STOP = 6,
        RED_MGR_EV_SHUTDOWN = 7
} red_mgr_event_t;

struct red_mgr_s {
        pthread_t runloop_tid;
        uint32_t runloop_created;
        uint32_t runloop_run;
        uint32_t start_req;
        uint32_t started;

        uint64_t epoch;
        red_mgr_state_t state;

        red_mgr_cfg_t cfg;
        red_mgr_cb_t cb;

        mgr_bus_t *bus;

        fsm_t *fsm;
        dispatch_t *dispatch;
        obs_t *obs;

        dispatch_qnode_t *dispatch_qmem;
        obs_evt_t *obs_ring_mem;
        uint32_t dispatch_qcap;
        uint32_t obs_ring_cap;

        red_engine_t *engine;
        red_shm_ra_t *shm_ra;
        uint32_t heartbeat_seq;
};

extern int red_mgr_build_fsm(red_mgr_t *m);
extern int red_mgr_build_dispatch(red_mgr_t *m);
extern int red_mgr_build_obs(red_mgr_t *m);
extern int red_mgr_handle_dispatch_result(red_mgr_t *m, const dispatch_pop_result_t *res);

extern void red_mgr_begin_new_epoch(red_mgr_t *m);
extern void red_mgr_cancel_all(red_mgr_t *m, uint32_t flush_q);

#endif /* __MGR_RED_RED_MGR_PRIV_H__ */