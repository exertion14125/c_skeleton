#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include "mgr/cfg/cfg_mgr_priv.h"
#include "mgr/contract/mgr_addrs.h"
#include "mgr/contract/mgr_msg_codes.h"

#define CFG_DISPATCH_QCAP    64U
#define CFG_OBS_RING_CAP     128U

static uint64_t cfg_now_ms(void *user)
{
        struct timespec ts;
        (void)user;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static int cfg_guard_always(void *ctx, fsm_state_t st, fsm_event_t ev, fsm_state_t next)
{
        (void)ctx;
        (void)st;
        (void)ev;
        (void)next;
        return 1;
}

static dispatch_decision_t cfg_dispatch_decide(void *user, const dispatch_t *d, const dispatch_evt_t *evt)
{
        (void)user;
        (void)d;
        (void)evt;
        return DISPATCH_DECIDE_CONSUME;
}

static int cfg_mgr_process_dispatch(cfg_mgr_t *m, uint32_t wait_ms, uint32_t budget)
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
                (void)cfg_mgr_handle_dispatch_result(m, &res);
                n++;
        }

        return 0;
}

static int cfg_mgr_send_rsp(cfg_mgr_t *m, uint16_t code, uint32_t req_id, int32_t rc)
{
        if (!m || !m->bus) {
                return -EINVAL;
        }
        return mgr_bus_send(m->bus,
                            APP_MGR_ADDR_CFG,
                            APP_MGR_ADDR_SYS,
                            code,
                            (int32_t)req_id,
                            rc,
                            cfg_now_ms(NULL));
}

static uint16_t cfg_mgr_rsp_code_from_action(cfg_eng_action_t action)
{
        switch (action) {
        case CFG_ENG_ACT_OPEN_DONE:
                return APP_MGR_MSG_CFG_OPEN_RSP;
        case CFG_ENG_ACT_ADJUST_DONE:
                return APP_MGR_MSG_CFG_ADJUST_RSP;
        case CFG_ENG_ACT_REOPEN_DONE:
                return APP_MGR_MSG_CFG_REOPEN_RSP;
        case CFG_ENG_ACT_MODIFY_DONE:
                return APP_MGR_MSG_CFG_MODIFY_RSP;
        case CFG_ENG_ACT_KEEP:
        case CFG_ENG_ACT_NONE:
        default:
                return APP_MGR_MSG_NONE;
        }
}

static int cfg_mgr_apply_engine(cfg_mgr_t *m,
                                const cfg_eng_input_t *in)
{
        cfg_eng_output_t out;
        uint16_t rsp_code;

        if (!m || !m->engine || !in) {
                return -EINVAL;
        }

        memset(&out, 0, sizeof(out));

        if (cfg_engine_apply(m->engine, in, &out) != 0) {
                return -1;
        }

        rsp_code = cfg_mgr_rsp_code_from_action(out.action);
        if (rsp_code == APP_MGR_MSG_NONE) {
                return 0;
        }

        return cfg_mgr_send_rsp(m, rsp_code, out.req_id, out.rc);
}

static int cfg_mgr_handle_bus_msg(cfg_mgr_t *m, const mgr_bus_msg_t *msg)
{
        cfg_eng_input_t in;

        if (!m || !msg) {
                return -EINVAL;
        }
        if (!m->engine) {
                return -EINVAL;
        }

        memset(&in, 0, sizeof(in));
        in.req_id = (uint32_t)msg->a;
        in.now_ms = cfg_now_ms(NULL);

        switch (msg->code) {
        case APP_MGR_MSG_SYS_CFG_OPEN_REQ:
                in.kind = CFG_ENG_IN_OPEN_REQ;
                break;

        case APP_MGR_MSG_SYS_CFG_ADJUST_REQ:
                in.kind = CFG_ENG_IN_ADJUST_REQ;
                in.arg0 = msg->b;
                break;

        case APP_MGR_MSG_SYS_CFG_REOPEN_REQ:
                in.kind = CFG_ENG_IN_REOPEN_REQ;
                break;

        case APP_MGR_MSG_SYS_CFG_MODIFY_REQ:
                in.kind = CFG_ENG_IN_MODIFY_REQ;
                in.arg0 = msg->a;
                in.arg1 = msg->b;
                break;

        default:
                return 0;
        }

        return cfg_mgr_apply_engine(m, &in);
}

cfg_mgr_t *alloc_cfg_mgr(void)
{
        cfg_mgr_t *m = (cfg_mgr_t *)calloc(1, sizeof(*m));
        if (!m) {
                return NULL;
        }

        m->state = CFG_ST_INIT;
        return m;
}

void destroy_cfg_mgr(cfg_mgr_t **pm)
{
        cfg_mgr_t *m;

        if (!pm || !*pm) {
                return;
        }

        m = *pm;
        *pm = NULL;

        deinit_cfg_mgr(m);
        free(m);
}

int init_cfg_mgr(cfg_mgr_t *m, const cfg_mgr_cfg_t *cfg, const cfg_mgr_cb_t *cb)
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

        if (cfg_mgr_build_fsm(m) != 0) {
                return -1;
        }
        if (cfg_mgr_build_dispatch(m) != 0) {
                return -1;
        }
        if (cfg_mgr_build_obs(m) != 0) {
                return -1;
        }

        m->engine = alloc_cfg_engine();
        if (!m->engine) {
                return -1;
        }

        if (init_cfg_engine(m->engine) != 0) {
                destroy_cfg_engine(&m->engine);
                return -1;
        }

        m->state = CFG_ST_IDLE;
        return 0;
}

int start_cfg_mgr(cfg_mgr_t *m)
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
                evt.ev = (fsm_event_t)CFG_MGR_EV_START;
                (void)dispatch_push(m->dispatch, &evt);
                (void)cfg_mgr_process_dispatch(m, 1U, 4U);
        }
        return 0;
}

void stop_cfg_mgr(cfg_mgr_t *m)
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
                evt.ev = (fsm_event_t)CFG_MGR_EV_STOP;
                (void)dispatch_push(m->dispatch, &evt);
                (void)cfg_mgr_process_dispatch(m, 1U, 4U);
        }
}

void deinit_cfg_mgr(cfg_mgr_t *m)
{
        if (!m) {
                return;
        }

        if (m->dispatch) {
                destroy_dispatch(&m->dispatch);
        }
        if (m->fsm) {
                destroy_fsm(&m->fsm);
        }
        if (m->obs) {
                destroy_obs(&m->obs);
        }
        if (m->engine) {
                destroy_cfg_engine(&m->engine);
        }

        free(m->dispatch_qmem);
        m->dispatch_qmem = NULL;
        free(m->obs_ring_mem);
        m->obs_ring_mem = NULL;

        m->bus = NULL;
        m->state = CFG_ST_SHUTDOWN;
}

cfg_mgr_state_t get_cfg_mgr_state(const cfg_mgr_t *m)
{
        if (!m) {
                return CFG_ST_ERR;
        }
        return m->state;
}

int cfg_mgr_poll_once(cfg_mgr_t *m, int timeout_ms)
{
        mgr_bus_msg_t bus_msg;

        if (!m) {
                return -EINVAL;
        }

        if (timeout_ms < 0) {
                timeout_ms = m->cfg.poll_timeout_ms;
        }

        if (m->state == CFG_ST_RUNNING && m->dispatch) {
                if (m->bus && mgr_bus_pop_for(m->bus, APP_MGR_ADDR_CFG, &bus_msg, 0) == 1) {
                        (void)cfg_mgr_handle_bus_msg(m, &bus_msg);
                }
                dispatch_evt_t evt;
                memset(&evt, 0, sizeof(evt));
                evt.ev = (fsm_event_t)CFG_MGR_EV_TICK;
                (void)dispatch_push(m->dispatch, &evt);
                (void)cfg_mgr_process_dispatch(m, (uint32_t)((timeout_ms > 0) ? timeout_ms : 1), 8U);
        } else if (timeout_ms > 0) {
                usleep((useconds_t)timeout_ms * 1000U);
        }

        return 0;
}

int cfg_mgr_bind_bus(cfg_mgr_t *m, mgr_bus_t *bus)
{
        if (!m) {
                return -EINVAL;
        }
        m->bus = bus;
        return 0;
}

int cfg_mgr_build_fsm(cfg_mgr_t *m)
{
        static const fsm_trans_t tbl[] = {
                { CFG_ST_INIT,     CFG_MGR_EV_START,    CFG_ST_IDLE,     cfg_guard_always, NULL },
                { CFG_ST_IDLE,     CFG_MGR_EV_START,    CFG_ST_RUNNING,  cfg_guard_always, NULL },
                { CFG_ST_RUNNING,  CFG_MGR_EV_TICK,     CFG_ST_RUNNING,  cfg_guard_always, NULL },
                { CFG_ST_RUNNING,  CFG_MGR_EV_TIMEOUT,  CFG_ST_ERR,      cfg_guard_always, NULL },
                { CFG_ST_RUNNING,  CFG_MGR_EV_ERROR,    CFG_ST_ERR,      cfg_guard_always, NULL },
                { CFG_ST_ERR,      CFG_MGR_EV_RECOVER,  CFG_ST_IDLE,     cfg_guard_always, NULL },
                { CFG_ST_IDLE,     CFG_MGR_EV_STOP,     CFG_ST_SHUTDOWN, cfg_guard_always, NULL },
                { CFG_ST_RUNNING,  CFG_MGR_EV_STOP,     CFG_ST_SHUTDOWN, cfg_guard_always, NULL },
                { CFG_ST_ERR,      CFG_MGR_EV_STOP,     CFG_ST_SHUTDOWN, cfg_guard_always, NULL }
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
        spec.init_state = CFG_ST_INIT;

        if (fsm_init(m->fsm, &spec, NULL) != 0) {
                return -1;
        }

        m->state = (cfg_mgr_state_t)fsm_get_state(m->fsm);
        return 0;
}

int cfg_mgr_build_dispatch(cfg_mgr_t *m)
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
                m->dispatch_qcap = CFG_DISPATCH_QCAP;
                m->dispatch_qmem = (dispatch_qnode_t *)calloc(m->dispatch_qcap, sizeof(dispatch_qnode_t));
                if (!m->dispatch_qmem) {
                        return -1;
                }
        }

        memset(&policy, 0, sizeof(policy));
        policy.decide = cfg_dispatch_decide;

        memset(&cfg, 0, sizeof(cfg));
        cfg.drop_on_full = 1;

        if (dispatch_init(m->dispatch, m->dispatch_qmem, m->dispatch_qcap, &policy, NULL, &cfg) != 0) {
                return -1;
        }

        return 0;
}

int cfg_mgr_build_obs(cfg_mgr_t *m)
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
                m->obs_ring_cap = CFG_OBS_RING_CAP;
                m->obs_ring_mem = (obs_evt_t *)calloc(m->obs_ring_cap, sizeof(obs_evt_t));
                if (!m->obs_ring_mem) {
                        return -1;
                }
        }

        if (obs_init(m->obs, m->obs_ring_mem, m->obs_ring_cap, cfg_now_ms, NULL) != 0) {
                return -1;
        }

        return 0;
}

int cfg_mgr_handle_dispatch_result(cfg_mgr_t *m, const dispatch_pop_result_t *res)
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
                m->state = (cfg_mgr_state_t)fsm_get_state(m->fsm);
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

void cfg_mgr_begin_new_epoch(cfg_mgr_t *m)
{
        if (!m) {
                return;
        }
        m->epoch++;
        if (m->obs) {
                obs_epoch_inc(m->obs);
        }
}

void cfg_mgr_cancel_all(cfg_mgr_t *m, uint32_t flush_q)
{
        if (!m) {
                return;
        }
        if (m->dispatch) {
                dispatch_cancel_all(m->dispatch, (flush_q ? true : false), 1U);
                dispatch_wakeup(m->dispatch);
        }
}