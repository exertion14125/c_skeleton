#ifndef __MGR_GIO_GIO_MGR_PRIV_H__
#define __MGR_GIO_GIO_MGR_PRIV_H__

#include <stdint.h>
#include <pthread.h>

#include "mgr/gio/gio_mgr.h"
#include "mgr/gio/gio_ipc_shm.h"
#include "mgr/gio/gio_ipc_sem.h"
#include "util/mgr_bus/mgr_bus.h"
#include "util/fsm/fsm.h"
#include "util/dispatch/dispatch.h"
#include "util/obs/obs.h"

typedef enum gio_mgr_event_e {
        GIO_MGR_EV_NONE = 0,
        GIO_MGR_EV_START = 1,
        GIO_MGR_EV_TICK = 2,
        GIO_MGR_EV_TIMEOUT = 3,
        GIO_MGR_EV_ERROR = 4,
        GIO_MGR_EV_RECOVER = 5,
        GIO_MGR_EV_STOP = 6,
        GIO_MGR_EV_SHUTDOWN = 7
} gio_mgr_event_t;

struct gio_mgr_s {
        pthread_t runloop_tid;
        uint32_t runloop_created;
        uint32_t runloop_run;
        uint32_t start_req;
        uint32_t started;

        uint64_t epoch;
        gio_mgr_state_t state;

        gio_mgr_cfg_t cfg;
        gio_mgr_cb_t cb;

        mgr_bus_t *bus;

        fsm_t *fsm;
        dispatch_t *dispatch;
        obs_t *obs;

        dispatch_qnode_t *dispatch_qmem;
        obs_evt_t *obs_ring_mem;
        uint32_t dispatch_qcap;
        uint32_t obs_ring_cap;

        gio_shm_t shm;
        gio_sem_t sem;
        uint32_t max_retry;
        uint32_t timeout_ms;
};

extern int gio_mgr_build_fsm(gio_mgr_t *m);
extern int gio_mgr_build_dispatch(gio_mgr_t *m);
extern int gio_mgr_build_obs(gio_mgr_t *m);
extern int gio_mgr_handle_dispatch_result(gio_mgr_t *m, const dispatch_pop_result_t *res);

extern void gio_mgr_begin_new_epoch(gio_mgr_t *m);
extern void gio_mgr_cancel_all(gio_mgr_t *m, uint32_t flush_q);

#endif /* __MGR_GIO_GIO_MGR_PRIV_H__ */