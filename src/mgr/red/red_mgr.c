#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include "mgr/red/red_mgr_priv.h"
#include "mgr/contract/mgr_addrs.h"
#include "mgr/contract/mgr_msg_codes.h"
#include "resource/red/red_state_dto.h"

#define RED_DISPATCH_QCAP    64U
#define RED_OBS_RING_CAP     128U

static uint64_t red_now_ms(void *user);

static int red_mgr_publish_state(red_mgr_t *m,
                                 const red_eng_output_t *out,
                                 uint32_t role_hint)
{
        red_state_dto_t dto;

        if (!m || !m->shm_ra || !out) {
                return -EINVAL;
        }

        memset(&dto, 0, sizeof(dto));
        dto.role = role_hint;
        dto.peer_state = out->reason_code;
        dto.health = (out->action == RED_ENG_ACT_PROPOSE_FAILOVER) ? 0U : 1U;
        dto.heartbeat_seq = ++m->heartbeat_seq;
        dto.ts_ms = red_now_ms(NULL);
        dto.flags = out->risk_flags;

        return red_shm_ra_write_state(m->shm_ra, &dto);
}

static uint64_t red_now_ms(void *user)
{
        struct timespec ts;
        (void)user;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static int red_guard_always(void *ctx, fsm_state_t st, fsm_event_t ev, fsm_state_t next)
{
        (void)ctx;
        (void)st;
        (void)ev;
        (void)next;
        return 1;
}

static dispatch_decision_t red_dispatch_decide(void *user, const dispatch_t *d, const dispatch_evt_t *evt)
{
        (void)user;
        (void)d;
        (void)evt;
        return DISPATCH_DECIDE_CONSUME;
}

static int red_mgr_process_dispatch(red_mgr_t *m, uint32_t wait_ms, uint32_t budget)
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
                (void)red_mgr_handle_dispatch_result(m, &res);
                n++;
        }

        return 0;
}

static int red_mgr_send_rsp(red_mgr_t *m, uint16_t code, uint32_t req_id, int32_t rc)
{
        if (!m || !m->bus) {
                return -EINVAL;
        }

        return mgr_bus_send(m->bus,
                            APP_MGR_ADDR_RED,
                            APP_MGR_ADDR_SYS,
                            code,
                            (int32_t)req_id,
                            rc,
                            red_now_ms(NULL));
}

static int red_mgr_send_evt(red_mgr_t *m, uint16_t code, uint32_t req_id, int32_t value)
{
        if (!m || !m->bus) {
                return -EINVAL;
        }

        return mgr_bus_send(m->bus,
                            APP_MGR_ADDR_RED,
                            APP_MGR_ADDR_SYS,
                            code,
                            (int32_t)req_id,
                            value,
                            red_now_ms(NULL));
}

static int red_mgr_handle_engine_output(red_mgr_t *m, const red_eng_output_t *out)
{
        if (!m || !out) {
                return -EINVAL;
        }

        (void)red_mgr_publish_state(m, out, (uint32_t)out->value);

        switch (out->action) {
        case RED_ENG_ACT_PROPOSE_FAILOVER:
                return red_mgr_send_evt(m,
                                        APP_MGR_MSG_RED_PROPOSE_FAILOVER_EVT,
                                        out->req_id,
                                        out->value);

        case RED_ENG_ACT_PROPOSE_RECOVER:
                return red_mgr_send_evt(m,
                                        APP_MGR_MSG_RED_PROPOSE_RECOVER_EVT,
                                        out->req_id,
                                        out->value);

        case RED_ENG_ACT_PROPOSE_HOLD:
                return red_mgr_send_evt(m,
                                        APP_MGR_MSG_RED_PROPOSE_HOLD_EVT,
                                        out->req_id,
                                        out->value);

        case RED_ENG_ACT_KEEP:
                return red_mgr_send_rsp(m,
                                        APP_MGR_MSG_RED_DECISION_RSP,
                                        out->req_id,
                                        out->value);

        case RED_ENG_ACT_PROPOSE_SAFE:
        case RED_ENG_ACT_NONE:
        default:
                return 0;
        }
}

static int red_mgr_handle_bus_msg(red_mgr_t *m, const mgr_bus_msg_t *msg)
{
        red_eng_input_t in;
        red_eng_output_t out;

        if (!m || !msg) {
                return -EINVAL;
        }
        if (!m->engine) {
                return -EINVAL;
        }

        memset(&in, 0, sizeof(in));
        memset(&out, 0, sizeof(out));

        if (msg->code == APP_MGR_MSG_SYS_RED_EVAL_REQ) {
                in.kind = RED_ENG_IN_EVAL_REQ;
                in.req_id = (uint32_t)msg->a;
                in.arg0 = msg->b;
                in.arg1 = 0;
                in.now_ms = red_now_ms(NULL);
        } else if (msg->code == APP_MGR_MSG_GIO_RED_INPUT_EVT) {
                /*
                 * 현재는 link_state 요약값만 받는 최소 계약.
                 * link_state == 0 이면 error 쪽, 아니면 ok 쪽으로 단순 매핑.
                 */
                in.kind = (msg->b == 0) ? RED_ENG_IN_PEER_ERR : RED_ENG_IN_PEER_OK;
                in.req_id = (uint32_t)msg->a;
                in.arg0 = msg->b;
                in.arg1 = 0;
                in.now_ms = red_now_ms(NULL);
        } else {
                return 0;
        }

        if (red_engine_apply(m->engine, &in, &out) != 0) {
                return red_mgr_send_rsp(m,
                                        APP_MGR_MSG_RED_DECISION_RSP,
                                        (uint32_t)msg->a,
                                        (int32_t)RED_DECISION_NONE);
        }

        return red_mgr_handle_engine_output(m, &out);
}

static int red_mgr_run_autonomous_eval(red_mgr_t *m)
{
        red_eng_input_t in;
        red_eng_output_t out;

        if (!m || !m->engine) {
                return -EINVAL;
        }

        memset(&in, 0, sizeof(in));
        memset(&out, 0, sizeof(out));

        in.kind = RED_ENG_IN_TICK_EVAL;
        in.req_id = 0U;
        in.arg0 = 0;
        in.arg1 = 0;
        in.now_ms = red_now_ms(NULL);

        if (red_engine_apply(m->engine, &in, &out) != 0) {
                return -1;
        }

        return red_mgr_handle_engine_output(m, &out);
}

red_mgr_t *alloc_red_mgr(void)
{
        red_mgr_t *m = (red_mgr_t *)calloc(1, sizeof(*m));
        if (!m) {
                return NULL;
        }

        m->state = RED_ST_INIT;
        return m;
}

void destroy_red_mgr(red_mgr_t **pm)
{
        red_mgr_t *m;

        if (!pm || !*pm) {
                return;
        }

        m = *pm;
        *pm = NULL;

        deinit_red_mgr(m);
        free(m);
}

int init_red_mgr(red_mgr_t *m, const red_mgr_cfg_t *cfg, const red_mgr_cb_t *cb)
{
        red_shm_ra_cfg_t shm_cfg;

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

        if (red_mgr_build_fsm(m) != 0) {
                return -1;
        }
        if (red_mgr_build_dispatch(m) != 0) {
                return -1;
        }
        if (red_mgr_build_obs(m) != 0) {
                return -1;
        }

        m->engine = alloc_red_engine();
        if (!m->engine) {
                return -1;
        }

        if (init_red_engine(m->engine) != 0) {
                destroy_red_engine(&m->engine);
                return -1;
        }

        m->shm_ra = alloc_red_shm_ra();
        if (!m->shm_ra) {
                destroy_red_engine(&m->engine);
                return -1;
        }

        memset(&shm_cfg, 0, sizeof(shm_cfg));
        shm_cfg.shm_name = "/skeleton_gio_shm";

        if (init_red_shm_ra(m->shm_ra, &shm_cfg) != 0 ||
            red_shm_ra_connect(m->shm_ra) != 0) {
                destroy_red_shm_ra(&m->shm_ra);
                destroy_red_engine(&m->engine);
                return -1;
        }

        m->heartbeat_seq = 0U;

        m->state = RED_ST_IDLE;
        return 0;
}

int start_red_mgr(red_mgr_t *m)
{
        int rc = 0;
        red_eng_output_t init_out;

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
                evt.ev = (fsm_event_t)RED_MGR_EV_START;
                (void)dispatch_push(m->dispatch, &evt);
                (void)red_mgr_process_dispatch(m, 1U, 4U);
        }

        memset(&init_out, 0, sizeof(init_out));
        init_out.action = RED_ENG_ACT_KEEP;
        init_out.value = 0;
        (void)red_mgr_publish_state(m, &init_out, 0U);

        return 0;
}

void stop_red_mgr(red_mgr_t *m)
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
                evt.ev = (fsm_event_t)RED_MGR_EV_STOP;
                (void)dispatch_push(m->dispatch, &evt);
                (void)red_mgr_process_dispatch(m, 1U, 4U);
        }
}

void deinit_red_mgr(red_mgr_t *m)
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
                destroy_red_engine(&m->engine);
        }
        if (m->shm_ra) {
                destroy_red_shm_ra(&m->shm_ra);
        }

        free(m->dispatch_qmem);
        m->dispatch_qmem = NULL;
        free(m->obs_ring_mem);
        m->obs_ring_mem = NULL;

        m->bus = NULL;
        m->state = RED_ST_SHUTDOWN;
}

red_mgr_state_t get_red_mgr_state(const red_mgr_t *m)
{
        if (!m) {
                return RED_ST_ERR;
        }
        return m->state;
}

int red_mgr_poll_once(red_mgr_t *m, int timeout_ms)
{
        mgr_bus_msg_t bus_msg;
        uint32_t bus_budget = 8U;
        uint32_t n = 0U;

        if (!m) {
                return -EINVAL;
        }

        if (timeout_ms < 0) {
                timeout_ms = m->cfg.poll_timeout_ms;
        }

        if (m->state == RED_ST_RUNNING && m->dispatch) {
                if (m->bus) {
                        while (n < bus_budget) {
                                if (mgr_bus_pop_for(m->bus,
                                                    APP_MGR_ADDR_RED,
                                                    &bus_msg,
                                                    0) != 1) {
                                        break;
                                }
                                (void)red_mgr_handle_bus_msg(m, &bus_msg);
                                n++;
                        }
                }

                (void)red_mgr_run_autonomous_eval(m);

                {
                        dispatch_evt_t evt;
                        memset(&evt, 0, sizeof(evt));
                        evt.ev = (fsm_event_t)RED_MGR_EV_TICK;
                        (void)dispatch_push(m->dispatch, &evt);
                }

                (void)red_mgr_process_dispatch(m,
                                               (uint32_t)((timeout_ms > 0) ? timeout_ms : 1),
                                               8U);
        } else if (timeout_ms > 0) {
                usleep((useconds_t)timeout_ms * 1000U);
        }

        return 0;
}

int red_mgr_bind_bus(red_mgr_t *m, mgr_bus_t *bus)
{
        if (!m) {
                return -EINVAL;
        }

        m->bus = bus;
        return 0;
}

int red_mgr_build_fsm(red_mgr_t *m)
{
        static const fsm_trans_t tbl[] = {
                { RED_ST_INIT,     RED_MGR_EV_START,    RED_ST_IDLE,     red_guard_always, NULL },
                { RED_ST_IDLE,     RED_MGR_EV_START,    RED_ST_RUNNING,  red_guard_always, NULL },
                { RED_ST_RUNNING,  RED_MGR_EV_TICK,     RED_ST_RUNNING,  red_guard_always, NULL },
                { RED_ST_RUNNING,  RED_MGR_EV_TIMEOUT,  RED_ST_ERR,      red_guard_always, NULL },
                { RED_ST_RUNNING,  RED_MGR_EV_ERROR,    RED_ST_ERR,      red_guard_always, NULL },
                { RED_ST_ERR,      RED_MGR_EV_RECOVER,  RED_ST_IDLE,     red_guard_always, NULL },
                { RED_ST_IDLE,     RED_MGR_EV_STOP,     RED_ST_SHUTDOWN, red_guard_always, NULL },
                { RED_ST_RUNNING,  RED_MGR_EV_STOP,     RED_ST_SHUTDOWN, red_guard_always, NULL },
                { RED_ST_ERR,      RED_MGR_EV_STOP,     RED_ST_SHUTDOWN, red_guard_always, NULL }
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
        spec.init_state = RED_ST_INIT;

        if (fsm_init(m->fsm, &spec, NULL) != 0) {
                return -1;
        }

        m->state = (red_mgr_state_t)fsm_get_state(m->fsm);
        return 0;
}

int red_mgr_build_dispatch(red_mgr_t *m)
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
                m->dispatch_qcap = RED_DISPATCH_QCAP;
                m->dispatch_qmem = (dispatch_qnode_t *)calloc(m->dispatch_qcap,
                                                              sizeof(dispatch_qnode_t));
                if (!m->dispatch_qmem) {
                        return -1;
                }
        }

        memset(&policy, 0, sizeof(policy));
        policy.decide = red_dispatch_decide;

        memset(&cfg, 0, sizeof(cfg));
        cfg.drop_on_full = 1;

        if (dispatch_init(m->dispatch,
                          m->dispatch_qmem,
                          m->dispatch_qcap,
                          &policy,
                          NULL,
                          &cfg) != 0) {
                return -1;
        }

        return 0;
}

int red_mgr_build_obs(red_mgr_t *m)
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
                m->obs_ring_cap = RED_OBS_RING_CAP;
                m->obs_ring_mem = (obs_evt_t *)calloc(m->obs_ring_cap, sizeof(obs_evt_t));
                if (!m->obs_ring_mem) {
                        return -1;
                }
        }

        if (obs_init(m->obs, m->obs_ring_mem, m->obs_ring_cap, red_now_ms, NULL) != 0) {
                return -1;
        }

        return 0;
}

int red_mgr_handle_dispatch_result(red_mgr_t *m, const dispatch_pop_result_t *res)
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
                m->state = (red_mgr_state_t)fsm_get_state(m->fsm);

                if (m->obs) {
                        (void)obs_push(m->obs,
                                       OBS_EVT_DISPATCH,
                                       (int32_t)res->evt.ev,
                                       (int32_t)res->evt.req_id,
                                       (int32_t)frc);
                        (void)obs_push(m->obs,
                                       OBS_EVT_FSM_TR,
                                       (int32_t)before,
                                       (int32_t)res->evt.ev,
                                       (int32_t)m->state);
                }
                break;

        case DISPATCH_POP_DROP_EPOCH:
                if (m->obs) {
                        (void)obs_push(m->obs,
                                       OBS_EVT_DROP_EPOCH,
                                       (int32_t)res->evt.ev,
                                       (int32_t)res->evt.req_id,
                                       0);
                }
                break;

        case DISPATCH_POP_DROP_REQ:
                if (m->obs) {
                        (void)obs_push(m->obs,
                                       OBS_EVT_DROP_REQ,
                                       (int32_t)res->evt.ev,
                                       (int32_t)res->evt.req_id,
                                       0);
                }
                break;

        case DISPATCH_POP_DROP_POLICY:
                if (m->obs) {
                        (void)obs_push(m->obs,
                                       OBS_EVT_DROP_POLICY,
                                       (int32_t)res->evt.ev,
                                       (int32_t)res->evt.req_id,
                                       0);
                }
                break;

        case DISPATCH_POP_DEFER:
                if (m->obs) {
                        (void)obs_push(m->obs,
                                       OBS_EVT_DEFER,
                                       (int32_t)res->evt.ev,
                                       (int32_t)res->evt.req_id,
                                       res->defer_reason);
                }
                break;

        default:
                break;
        }

        return 0;
}

void red_mgr_begin_new_epoch(red_mgr_t *m)
{
        if (!m) {
                return;
        }

        m->epoch++;
        if (m->obs) {
                obs_epoch_inc(m->obs);
        }
}

void red_mgr_cancel_all(red_mgr_t *m, uint32_t flush_q)
{
        if (!m) {
                return;
        }

        if (m->dispatch) {
                dispatch_cancel_all(m->dispatch, (flush_q ? true : false), 1U);
                dispatch_wakeup(m->dispatch);
        }
}