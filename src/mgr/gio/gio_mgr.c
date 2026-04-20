#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include "mgr/gio/gio_mgr_priv.h"
#include "mgr/contract/mgr_addrs.h"
#include "mgr/contract/mgr_msg_codes.h"

#define GIO_DISPATCH_QCAP    64U
#define GIO_OBS_RING_CAP     128U

static uint64_t gio_now_ms(void *user)
{
        struct timespec ts;
        (void)user;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static int gio_guard_always(void *ctx, fsm_state_t st, fsm_event_t ev, fsm_state_t next)
{
        (void)ctx;
        (void)st;
        (void)ev;
        (void)next;
        return 1;
}

static dispatch_decision_t gio_dispatch_decide(void *user, const dispatch_t *d, const dispatch_evt_t *evt)
{
        (void)user;
        (void)d;
        (void)evt;
        return DISPATCH_DECIDE_CONSUME;
}

static int gio_mgr_process_dispatch(gio_mgr_t *m, uint32_t wait_ms, uint32_t budget)
{
        uint32_t n = 0;
        dispatch_pop_result_t res;

        if (!m || !m->dispatch) {
                return -EINVAL;
        }

        (void)dispatch_wait(m->dispatch, wait_ms);
        while (n < budget) {
                dispatch_pop_rc_t prc = dispatch_pop(m->dispatch, &res);
                if (prc == DISPATCH_POP_EMPTY) {
                        break;
                }
                (void)gio_mgr_handle_dispatch_result(m, &res);
                n++;
        }

        return 0;
}

static int gio_mgr_send_rsp(gio_mgr_t *m, uint16_t code, uint32_t req_id, int32_t rc)
{
        if (!m || !m->bus) {
                return -EINVAL;
        }
        return mgr_bus_send(m->bus,
                            APP_MGR_ADDR_GIO,
                            APP_MGR_ADDR_SYS,
                            code,
                            (int32_t)req_id,
                            rc,
                            gio_now_ms(NULL));
}

static int gio_mgr_ipc_connect(gio_mgr_t *m)
{
        gio_sem_cfg_t scfg;

        if (!m) {
                return -EINVAL;
        }

        memset(&scfg, 0, sizeof(scfg));
        scfg.req_name = "/skeleton_gio_req_sem";
        scfg.rsp_name = "/skeleton_gio_rsp_sem";
        scfg.req_init = 0U;
        scfg.rsp_init = 0U;

        if (gio_shm_open(&m->shm, "/skeleton_gio_shm", 0U) != 0) {
                return -1;
        }
        if (gio_shm_map(&m->shm) != 0) {
                (void)gio_shm_close(&m->shm);
                return -1;
        }
        if (gio_sem_open(&m->sem, &scfg) != 0) {
                (void)gio_shm_close(&m->shm);
                return -1;
        }

        return 0;
}

static void gio_mgr_ipc_disconnect(gio_mgr_t *m)
{
        if (!m) {
                return;
        }
        (void)gio_sem_close(&m->sem);
        (void)gio_shm_close(&m->shm);
}

static int gio_mgr_exec_ipc(gio_mgr_t *m, uint32_t req_id, int32_t arg)
{
        uint32_t n;
        int rc = -1;
        gio_ipc_req_t req;
        gio_ipc_rsp_t rsp;

        if (!m) {
                return -EINVAL;
        }

        memset(&req, 0, sizeof(req));
        req.req_id = req_id;
        req.arg = arg;

        for (n = 0; n <= m->max_retry; ++n) {
                if (gio_shm_write_req(&m->shm, &req) != 0) {
                        rc = -1;
                        continue;
                }
                if (gio_sem_post_req(&m->sem) != 0) {
                        rc = -1;
                        continue;
                }
                rc = gio_sem_wait_rsp(&m->sem, m->timeout_ms);
                if (rc == 0) {
                        if (gio_shm_read_rsp(&m->shm, &rsp) == 0) {
                                return rsp.rc;
                        }
                        rc = -1;
                }
        }

        return rc;
}

static int gio_mgr_handle_bus_msg(gio_mgr_t *m, const mgr_bus_msg_t *msg)
{
        int rc;

        if (!m || !msg) {
                return -EINVAL;
        }

        if (msg->code != APP_MGR_MSG_SYS_GIO_EXEC_REQ) {
                return 0;
        }

        rc = gio_mgr_exec_ipc(m, (uint32_t)msg->a, msg->b);
        if (rc == 0) {
                return gio_mgr_send_rsp(m, APP_MGR_MSG_GIO_EXEC_RSP, (uint32_t)msg->a, 0);
        }

        (void)gio_mgr_send_rsp(m, APP_MGR_MSG_GIO_TIMEOUT_EVT, (uint32_t)msg->a, rc);
        return gio_mgr_send_rsp(m, APP_MGR_MSG_GIO_DEGRADED_EVT, (uint32_t)msg->a, rc);
}

gio_mgr_t *alloc_gio_mgr(void)
{
        gio_mgr_t *m = (gio_mgr_t *)calloc(1, sizeof(*m));
        if (!m) {
                return NULL;
        }

        m->state = GIO_ST_INIT;
        return m;
}

void destroy_gio_mgr(gio_mgr_t **pm)
{
        gio_mgr_t *m;

        if (!pm || !*pm) {
                return;
        }

        m = *pm;
        *pm = NULL;

        deinit_gio_mgr(m);
        free(m);
}

int init_gio_mgr(gio_mgr_t *m, const gio_mgr_cfg_t *cfg, const gio_mgr_cb_t *cb)
{
        if (!m) {
                return -EINVAL;
        }

        memset(&m->cfg, 0, sizeof(m->cfg));
        memset(&m->cb, 0, sizeof(m->cb));

        if (cfg) {
                m->cfg = *cfg;
        }
        if (cb) {
                m->cb = *cb;
        }

        if (m->cfg.poll_timeout_ms <= 0) {
                m->cfg.poll_timeout_ms = 100;
        }
        m->max_retry = 2U;
        m->timeout_ms = 100U;

        if (gio_mgr_build_fsm(m) != 0) {
                return -1;
        }
        if (gio_mgr_build_dispatch(m) != 0) {
                return -1;
        }
        if (gio_mgr_build_obs(m) != 0) {
                return -1;
        }
        if (gio_mgr_ipc_connect(m) != 0) {
                return -1;
        }

        m->state = GIO_ST_IDLE;
        return 0;
}

int start_gio_mgr(gio_mgr_t *m)
{
        int rc = 0;

        if (!m) {
                return -EINVAL;
        }

        if (m->cb.on_start) {
                rc = m->cb.on_start(m->cb.user);
                if (rc != 0) {
                        return rc;
                }
        }

        if (m->dispatch) {
                dispatch_evt_t evt;
                memset(&evt, 0, sizeof(evt));
                evt.ev = (fsm_event_t)GIO_MGR_EV_START;
                (void)dispatch_push(m->dispatch, &evt);
                (void)gio_mgr_process_dispatch(m, 1U, 4U);
        }
        return 0;
}

void stop_gio_mgr(gio_mgr_t *m)
{
        if (!m) {
                return;
        }

        if (m->cb.on_stop) {
                m->cb.on_stop(m->cb.user);
        }

        if (m->dispatch) {
                dispatch_evt_t evt;
                memset(&evt, 0, sizeof(evt));
                evt.ev = (fsm_event_t)GIO_MGR_EV_STOP;
                (void)dispatch_push(m->dispatch, &evt);
                (void)gio_mgr_process_dispatch(m, 1U, 4U);
        }
}

void deinit_gio_mgr(gio_mgr_t *m)
{
        if (!m) {
                return;
        }

        gio_mgr_ipc_disconnect(m);

        if (m->dispatch) {
                destroy_dispatch(&m->dispatch);
        }
        if (m->fsm) {
                destroy_fsm(&m->fsm);
        }
        if (m->obs) {
                destroy_obs(&m->obs);
        }

        free(m->dispatch_qmem);
        m->dispatch_qmem = NULL;
        free(m->obs_ring_mem);
        m->obs_ring_mem = NULL;

        m->bus = NULL;
        m->state = GIO_ST_SHUTDOWN;
}

gio_mgr_state_t get_gio_mgr_state(const gio_mgr_t *m)
{
        if (!m) {
                return GIO_ST_ERR;
        }
        return m->state;
}

int gio_mgr_poll_once(gio_mgr_t *m, int timeout_ms)
{
        mgr_bus_msg_t bus_msg;

        if (!m) {
                return -EINVAL;
        }

        if (timeout_ms < 0) {
                timeout_ms = m->cfg.poll_timeout_ms;
        }

        if (m->state == GIO_ST_RUNNING && m->dispatch) {
                if (m->bus && mgr_bus_pop_for(m->bus, APP_MGR_ADDR_GIO, &bus_msg, 0) == 1) {
                        (void)gio_mgr_handle_bus_msg(m, &bus_msg);
                }
                dispatch_evt_t evt;
                memset(&evt, 0, sizeof(evt));
                evt.ev = (fsm_event_t)GIO_MGR_EV_TICK;
                (void)dispatch_push(m->dispatch, &evt);
                (void)gio_mgr_process_dispatch(m, (uint32_t)((timeout_ms > 0) ? timeout_ms : 1), 8U);
        } else if (timeout_ms > 0) {
                usleep((useconds_t)timeout_ms * 1000U);
        }

        return 0;
}

int gio_mgr_bind_bus(gio_mgr_t *m, mgr_bus_t *bus)
{
        if (!m) {
                return -EINVAL;
        }
        m->bus = bus;
        return 0;
}

int gio_mgr_build_fsm(gio_mgr_t *m)
{
        static const fsm_trans_t tbl[] = {
                { GIO_ST_INIT,     GIO_MGR_EV_START,    GIO_ST_IDLE,     gio_guard_always, NULL },
                { GIO_ST_IDLE,     GIO_MGR_EV_START,    GIO_ST_RUNNING,  gio_guard_always, NULL },
                { GIO_ST_RUNNING,  GIO_MGR_EV_TICK,     GIO_ST_RUNNING,  gio_guard_always, NULL },
                { GIO_ST_RUNNING,  GIO_MGR_EV_TIMEOUT,  GIO_ST_ERR,      gio_guard_always, NULL },
                { GIO_ST_RUNNING,  GIO_MGR_EV_ERROR,    GIO_ST_ERR,      gio_guard_always, NULL },
                { GIO_ST_ERR,      GIO_MGR_EV_RECOVER,  GIO_ST_IDLE,     gio_guard_always, NULL },
                { GIO_ST_IDLE,     GIO_MGR_EV_STOP,     GIO_ST_SHUTDOWN, gio_guard_always, NULL },
                { GIO_ST_RUNNING,  GIO_MGR_EV_STOP,     GIO_ST_SHUTDOWN, gio_guard_always, NULL },
                { GIO_ST_ERR,      GIO_MGR_EV_STOP,     GIO_ST_SHUTDOWN, gio_guard_always, NULL }
        };
        fsm_spec_t spec;

        if (!m) {
                return -EINVAL;
        }
        if (!m->fsm) {
                m->fsm = alloc_fsm();
                if (!m->fsm) {
                        return -1;
                }
        }

        spec.table = tbl;
        spec.table_len = (uint32_t)(sizeof(tbl) / sizeof(tbl[0]));
        spec.init_state = GIO_ST_INIT;

        if (fsm_init(m->fsm, &spec, NULL) != 0) {
                return -1;
        }

        m->state = (gio_mgr_state_t)fsm_get_state(m->fsm);
        return 0;
}

int gio_mgr_build_dispatch(gio_mgr_t *m)
{
        dispatch_policy_t policy;
        dispatch_cfg_t cfg;

        if (!m) {
                return -EINVAL;
        }
        if (!m->dispatch) {
                m->dispatch = alloc_dispatch();
                if (!m->dispatch) {
                        return -1;
                }
        }

        if (!m->dispatch_qmem) {
                m->dispatch_qcap = GIO_DISPATCH_QCAP;
                m->dispatch_qmem = (dispatch_qnode_t *)calloc(m->dispatch_qcap, sizeof(dispatch_qnode_t));
                if (!m->dispatch_qmem) {
                        return -1;
                }
        }

        memset(&policy, 0, sizeof(policy));
        policy.decide = gio_dispatch_decide;

        memset(&cfg, 0, sizeof(cfg));
        cfg.drop_on_full = 1;

        if (dispatch_init(m->dispatch, m->dispatch_qmem, m->dispatch_qcap, &policy, NULL, &cfg) != 0) {
                return -1;
        }

        return 0;
}

int gio_mgr_build_obs(gio_mgr_t *m)
{
        if (!m) {
                return -EINVAL;
        }

        if (!m->obs) {
                m->obs = alloc_obs();
                if (!m->obs) {
                        return -1;
                }
        }

        if (!m->obs_ring_mem) {
                m->obs_ring_cap = GIO_OBS_RING_CAP;
                m->obs_ring_mem = (obs_evt_t *)calloc(m->obs_ring_cap, sizeof(obs_evt_t));
                if (!m->obs_ring_mem) {
                        return -1;
                }
        }

        if (obs_init(m->obs, m->obs_ring_mem, m->obs_ring_cap, gio_now_ms, NULL) != 0) {
                return -1;
        }

        return 0;
}

int gio_mgr_handle_dispatch_result(gio_mgr_t *m, const dispatch_pop_result_t *res)
{
        fsm_step_rc_t frc;
        fsm_state_t before;

        if (!m) {
                return -EINVAL;
        }
        if (!res) {
                return -EINVAL;
        }

        switch (res->rc) {
        case DISPATCH_POP_OK:
                if (!m->fsm) {
                        return -EINVAL;
                }
                before = fsm_get_state(m->fsm);
                frc = fsm_step(m->fsm, m, res->evt.ev);
                m->state = (gio_mgr_state_t)fsm_get_state(m->fsm);
                if (m->obs) {
                        (void)obs_push(m->obs, OBS_EVT_DISPATCH, (int32_t)res->evt.ev, (int32_t)res->evt.req_id, (int32_t)frc);
                        (void)obs_push(m->obs, OBS_EVT_FSM_TR, (int32_t)before, (int32_t)res->evt.ev, (int32_t)m->state);
                }
                break;
        case DISPATCH_POP_DROP_EPOCH:
                if (m->obs) {
                        (void)obs_push(m->obs, OBS_EVT_DROP_EPOCH, (int32_t)res->evt.ev, (int32_t)res->evt.req_id, 0);
                }
                break;
        case DISPATCH_POP_DROP_REQ:
                if (m->obs) {
                        (void)obs_push(m->obs, OBS_EVT_DROP_REQ, (int32_t)res->evt.ev, (int32_t)res->evt.req_id, 0);
                }
                break;
        case DISPATCH_POP_DROP_POLICY:
                if (m->obs) {
                        (void)obs_push(m->obs, OBS_EVT_DROP_POLICY, (int32_t)res->evt.ev, (int32_t)res->evt.req_id, 0);
                }
                break;
        case DISPATCH_POP_DEFER:
                if (m->obs) {
                        (void)obs_push(m->obs, OBS_EVT_DEFER, (int32_t)res->evt.ev, (int32_t)res->evt.req_id, res->defer_reason);
                }
                break;
        default:
                break;
        }

        return 0;
}

void gio_mgr_begin_new_epoch(gio_mgr_t *m)
{
        if (!m) {
                return;
        }
        m->epoch++;
        if (m->obs) {
                obs_epoch_inc(m->obs);
        }
}

void gio_mgr_cancel_all(gio_mgr_t *m, uint32_t flush_q)
{
        if (!m) {
                return;
        }
        if (m->dispatch) {
                dispatch_cancel_all(m->dispatch, (flush_q ? true : false), 1U);
                dispatch_wakeup(m->dispatch);
        }
}