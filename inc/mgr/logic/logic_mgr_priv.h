#ifndef __MGR_LOGIC_LOGIC_MGR_PRIV_H__
#define __MGR_LOGIC_LOGIC_MGR_PRIV_H__

#include <stdint.h>
#include <pthread.h>

#include "mgr/logic/logic_mgr.h"
#include "engine/logic/logic_engine.h"
#include "engine/logic/logic_output_map.h"
#include "ra/gio/gio_shm_ra.h"
#include "util/mgr_bus/mgr_bus.h"
#include "util/fsm/fsm.h"
#include "util/dispatch/dispatch.h"
#include "util/obs/obs.h"

typedef enum logic_mgr_event_e {
        LOGIC_MGR_EV_NONE = 0,
        LOGIC_MGR_EV_START = 1,
        LOGIC_MGR_EV_TICK = 2,
        LOGIC_MGR_EV_TIMEOUT = 3,
        LOGIC_MGR_EV_ERROR = 4,
        LOGIC_MGR_EV_RECOVER = 5,
        LOGIC_MGR_EV_STOP = 6,
        LOGIC_MGR_EV_SHUTDOWN = 7
} logic_mgr_event_t;

struct logic_mgr_s {
        pthread_t runloop_tid;
        uint32_t runloop_created;
        uint32_t runloop_run;
        uint32_t start_req;
        uint32_t started;

        uint64_t epoch;
        logic_mgr_state_t state;

        logic_mgr_cfg_t cfg;
        logic_mgr_cb_t cb;

        mgr_bus_t *bus;

        fsm_t *fsm;
        dispatch_t *dispatch;
        obs_t *obs;

        dispatch_qnode_t *dispatch_qmem;
        obs_evt_t *obs_ring_mem;
        uint32_t dispatch_qcap;
        uint32_t obs_ring_cap;

        logic_engine_t *engine;
        logic_output_map_t *out_map;
        gio_shm_ra_t *ra;
};

extern int logic_mgr_build_fsm(logic_mgr_t *m);
extern int logic_mgr_build_dispatch(logic_mgr_t *m);
extern int logic_mgr_build_obs(logic_mgr_t *m);
extern int logic_mgr_handle_dispatch_result(logic_mgr_t *m, const dispatch_pop_result_t *res);

extern void logic_mgr_begin_new_epoch(logic_mgr_t *m);
extern void logic_mgr_cancel_all(logic_mgr_t *m, uint32_t flush_q);

#endif /* __MGR_LOGIC_LOGIC_MGR_PRIV_H__ */