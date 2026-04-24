#ifndef __MGR_CFG_CFG_MGR_PRIV_H__
#define __MGR_CFG_CFG_MGR_PRIV_H__

#include <stdint.h>
#include <pthread.h>

#include "mgr/cfg/cfg_mgr.h"
#include "engine/cfg/cfg_engine.h"
#include "ra/cfg/cfg_request_ra.h"
#include "ra/cfg/cfg_shm_ra.h"
#include "util/mgr_bus/mgr_bus.h"
#include "util/fsm/fsm.h"
#include "util/dispatch/dispatch.h"
#include "util/obs/obs.h"

typedef enum cfg_mgr_event_e {
        CFG_MGR_EV_NONE = 0,
        CFG_MGR_EV_START = 1,
        CFG_MGR_EV_TICK = 2,
        CFG_MGR_EV_TIMEOUT = 3,
        CFG_MGR_EV_ERROR = 4,
        CFG_MGR_EV_RECOVER = 5,
        CFG_MGR_EV_STOP = 6,
        CFG_MGR_EV_SHUTDOWN = 7
} cfg_mgr_event_t;

typedef struct cfg_logic_map_cache_s {
        uint32_t valid;
        uint32_t out_card_no;
        uint32_t out_card_type;
        uint32_t out_ch0;
        uint32_t out_ch1;
} cfg_logic_map_cache_t;

struct cfg_mgr_s {
        pthread_t runloop_tid;
        uint32_t runloop_created;
        uint32_t runloop_run;
        uint32_t start_req;
        uint32_t started;

        uint64_t epoch;
        cfg_mgr_state_t state;

        cfg_mgr_cfg_t cfg;
        cfg_mgr_cb_t cb;

        mgr_bus_t *bus;

        fsm_t *fsm;
        dispatch_t *dispatch;
        obs_t *obs;

        dispatch_qnode_t *dispatch_qmem;
        obs_evt_t *obs_ring_mem;
        uint32_t dispatch_qcap;
        uint32_t obs_ring_cap;

        cfg_engine_t *engine;

        cfg_request_ra_t *request_ra;
        cfg_shm_ra_t *shm_ra;
        uint32_t cfg_generation;

        cfg_logic_map_cache_t logic_map_cache;
};

extern int cfg_mgr_build_fsm(cfg_mgr_t *m);
extern int cfg_mgr_build_dispatch(cfg_mgr_t *m);
extern int cfg_mgr_build_obs(cfg_mgr_t *m);
extern int cfg_mgr_handle_dispatch_result(cfg_mgr_t *m, const dispatch_pop_result_t *res);

extern void cfg_mgr_begin_new_epoch(cfg_mgr_t *m);
extern void cfg_mgr_cancel_all(cfg_mgr_t *m, uint32_t flush_q);

#endif /* __MGR_CFG_CFG_MGR_PRIV_H__ */