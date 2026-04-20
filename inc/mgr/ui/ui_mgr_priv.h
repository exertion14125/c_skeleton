#ifndef __MGR_UI_UI_MGR_PRIV_H__
#define __MGR_UI_UI_MGR_PRIV_H__

#include <stdint.h>
#include <pthread.h>

#include "mgr/ui/ui_mgr.h"
#include "mgr/ui/ui_proto.h"

#include "util/fsm/fsm.h"
#include "util/dispatch/dispatch.h"
#include "util/obs/obs.h"
// ========================= //
// internal events for FSM //
// public state enum is reused from ui_mgr.h: //
//   UI_ST_INIT / UI_ST_IDLE / UI_ST_HANDSHAKE / UI_ST_CONNECTED / UI_ST_SHUTDOWN //
// ========================= //

typedef enum ui_mgr_event_e {
        UI_MGR_EV_NONE = 0,
        UI_MGR_EV_START,
        UI_MGR_EV_OPEN_REQ,
        UI_MGR_EV_OPEN_OK,
        UI_MGR_EV_OPEN_FAIL,
        UI_MGR_EV_RX_REQ,
        UI_MGR_EV_RX_DONE,
        UI_MGR_EV_TX_REQ,
        UI_MGR_EV_TIMEOUT,
        UI_MGR_EV_RESET,
        UI_MGR_EV_SHUTDOWN,
        UI_MGR_EV_CANCEL_ALL,
        UI_MGR_EV_CANCEL_REQ
} ui_mgr_event_t;

/* RA forward declarations only */
typedef struct ra_ui_uds_srv_s ra_ui_uds_srv_t;
typedef struct ra_ui_shm_s ra_ui_shm_t;

// typedef struct ui_mgr_cfg_s ui_mgr_cfg_t;
// typedef struct ui_mgr_s ui_mgr_t;

typedef struct ui_mgr_ctx_s {
        uint32_t last_req_id;
        uint32_t last_err;
        uint32_t connected;

        uint64_t last_rx_ms;
        uint64_t last_tx_ms;
        uint64_t last_alive_ms;
} ui_mgr_ctx_t;

typedef struct ui_proto_ctx_s {
        int      await_pong;             /* 1 if ping sent and waiting for PONG */
        uint64_t pong_deadline_ms;       /* PONG deadline (ms) */
        uint64_t hello_last_seen_wseq;   /* last seen hello wseq */

        ui_rx_buf_t *rx_buf;             /* protocol rx assembly / parse buffer */
} ui_proto_ctx_t;

/// @brief UI manager aggregate root.
struct ui_mgr_s {
        /* generic manager main thread flags if needed later */
        pthread_t thr;
        uint32_t running;
        /* runloop lifecycle */
        pthread_t runloop_tid;
        uint32_t  runloop_created;
        uint32_t  runloop_run;
        uint32_t  start_req;
        uint32_t  started;
        /* logical generation */
        uint64_t epoch;

        /* manager-owned utilities */
        fsm_t *fsm;
        dispatch_t *dispatch;
        obs_t *obs;

        fsm_hook_t fsm_hook;
        dispatch_policy_t dispatch_policy;
        dispatch_observer_t dispatch_observer;

        dispatch_qnode_t *dispatch_qmem;
        obs_evt_t *obs_ring_mem;

        uint32_t dispatch_qcap;
        uint32_t obs_ring_cap;
        uint32_t loop_wait_ms;

        /* resource access */
        ra_ui_uds_srv_t *ra_uds_srv;
        ra_ui_shm_t *ra_shm;

        /* narrow notify callback only */
        ui_mgr_cb_t notify_cb;

        /* contexts */
        ui_mgr_ctx_t ctx;
        ui_proto_ctx_t proto;

        /* static configuration */
        ui_mgr_cfg_t cfg;
};
extern int ui_mgr_build_fsm(ui_mgr_t *m);
extern int ui_mgr_build_dispatch(ui_mgr_t *m);
extern int ui_mgr_build_obs(ui_mgr_t *m);

extern void ui_mgr_begin_new_epoch(ui_mgr_t *m);
extern void ui_mgr_cancel_all(ui_mgr_t *m, uint32_t flush_q);

// extern void *ui_mgr_runloop_main(void *arg);

extern int ui_mgr_handle_dispatch_result(ui_mgr_t *m, const dispatch_pop_result_t *res);
#endif /* __MGR_UI_UI_MGR_PRIV_H__ */