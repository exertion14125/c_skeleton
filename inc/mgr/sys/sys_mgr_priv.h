#ifndef __MGR_SYS_SYS_MGR_PRIV_H__
#define __MGR_SYS_SYS_MGR_PRIV_H__

#include <stdint.h>
#include <pthread.h>

#include "mgr/sys/sys_mgr.h"
#include "engine/sys/sys_engine.h"
#include "util/mgr_bus/mgr_bus.h"
#include "util/fsm/fsm.h"
#include "util/dispatch/dispatch.h"
#include "util/obs/obs.h"

typedef enum sys_mgr_event_e {
        SYS_MGR_EV_NONE = 0,
        SYS_MGR_EV_START = 1,
        SYS_MGR_EV_TICK = 2,
        SYS_MGR_EV_TIMEOUT = 3,
        SYS_MGR_EV_ERROR = 4,
        SYS_MGR_EV_RECOVER = 5,
        SYS_MGR_EV_STOP = 6,
        SYS_MGR_EV_SHUTDOWN = 7
} sys_mgr_event_t;

struct sys_mgr_s {
        pthread_t runloop_tid;
        uint32_t runloop_created;
        uint32_t runloop_run;
        uint32_t start_req;
        uint32_t started;

        uint64_t epoch;
        sys_mgr_state_t state;

        sys_mgr_cfg_t cfg;
        sys_mgr_cb_t cb;

        mgr_bus_t *bus;

        fsm_t *fsm;
        dispatch_t *dispatch;
        obs_t *obs;

        dispatch_qnode_t *dispatch_qmem;
        obs_evt_t *obs_ring_mem;
        uint32_t dispatch_qcap;
        uint32_t obs_ring_cap;

        sys_engine_t *engine;
};

extern int sys_mgr_build_fsm(sys_mgr_t *m);
extern int sys_mgr_build_dispatch(sys_mgr_t *m);
extern int sys_mgr_build_obs(sys_mgr_t *m);
extern int sys_mgr_handle_dispatch_result(sys_mgr_t *m, const dispatch_pop_result_t *res);

extern void sys_mgr_begin_new_epoch(sys_mgr_t *m);
extern void sys_mgr_cancel_all(sys_mgr_t *m, uint32_t flush_q);

#endif /* __MGR_SYS_SYS_MGR_PRIV_H__ */