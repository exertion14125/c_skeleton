#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include "mgr/logic/logic_mgr_priv.h"
#include "mgr/contract/mgr_addrs.h"
#include "mgr/contract/mgr_msg_codes.h"

#define LOGIC_DISPATCH_QCAP    64U
#define LOGIC_OBS_RING_CAP     128U

static uint64_t logic_now_ms(void *user)
{
        struct timespec ts;
        (void)user;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static int logic_guard_always(void *ctx, fsm_state_t st, fsm_event_t ev, fsm_state_t next)
{
        (void)ctx;
        (void)st;
        (void)ev;
        (void)next;
        return 1;
}

static dispatch_decision_t logic_dispatch_decide(void *user, const dispatch_t *d, const dispatch_evt_t *evt)
{
        (void)user;
        (void)d;
        (void)evt;
        return DISPATCH_DECIDE_CONSUME;
}

static int logic_mgr_process_dispatch(logic_mgr_t *m, uint32_t wait_ms, uint32_t budget)
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
                (void)logic_mgr_handle_dispatch_result(m, &res);
                n++;
        }

        return 0;
}

static int logic_mgr_send_rsp(logic_mgr_t *m, uint16_t code, uint32_t req_id, int32_t rc)
{
        if (!m || !m->bus) {
                return -EINVAL;
        }

        return mgr_bus_send(m->bus,
                            APP_MGR_ADDR_LOGIC,
                            APP_MGR_ADDR_SYS,
                            code,
                            (int32_t)req_id,
                            rc,
                            logic_now_ms(NULL));
}

static int logic_mgr_write_output_snapshot(logic_mgr_t *m,
                                           const gio_output_snapshot_t *out_snap)
{
        if (!m || !m->ra || !out_snap) {
                return -EINVAL;
        }

        return gio_shm_ra_write_output(m->ra, out_snap);
}

static int logic_mgr_handle_bus_msg(logic_mgr_t *m, const mgr_bus_msg_t *msg)
{
        logic_eng_output_t out;
        gio_input_snapshot_t in_snap;
        gio_output_snapshot_t out_snap;

        if (!m || !msg) {
                return -EINVAL;
        }
        if (!m->engine || !m->ra || !m->out_map) {
                return -EINVAL;
        }

        if (msg->code != APP_MGR_MSG_SYS_LOGIC_EXEC_REQ) {
                return 0;
        }

        memset(&in_snap, 0, sizeof(in_snap));
        memset(&out, 0, sizeof(out));
        memset(&out_snap, 0, sizeof(out_snap));

        if (gio_shm_ra_read_input(m->ra, &in_snap) != 0) {
                return logic_mgr_send_rsp(m,
                                          APP_MGR_MSG_LOGIC_EXEC_RSP,
                                          (uint32_t)msg->a,
                                          -1);
        }

        if (logic_engine_apply_snapshot(m->engine,
                                        &in_snap,
                                        logic_now_ms(NULL),
                                        &out) != 0) {
                return logic_mgr_send_rsp(m,
                                          APP_MGR_MSG_LOGIC_EXEC_RSP,
                                          (uint32_t)msg->a,
                                          -1);
        }

        if (logic_output_map_apply(m->out_map,
                                   &out,
                                   logic_now_ms(NULL),
                                   &out_snap) != 0) {
                return logic_mgr_send_rsp(m,
                                          APP_MGR_MSG_LOGIC_EXEC_RSP,
                                          (uint32_t)msg->a,
                                          -1);
        }

        (void)logic_mgr_write_output_snapshot(m, &out_snap);

        return logic_mgr_send_rsp(m,
                                  APP_MGR_MSG_LOGIC_EXEC_RSP,
                                  out.req_id,
                                  out.rc);
}

logic_mgr_t *alloc_logic_mgr(void)
{
        logic_mgr_t *m = (logic_mgr_t *)calloc(1, sizeof(*m));
        if (!m) {
                return NULL;
        }

        m->state = LOGIC_ST_INIT;
        return m;
}

void destroy_logic_mgr(logic_mgr_t **pm)
{
        logic_mgr_t *m;

        if (!pm || !*pm) {
                return;
        }

        m = *pm;
        *pm = NULL;

        deinit_logic_mgr(m);
        free(m);
}

int init_logic_mgr(logic_mgr_t *m, const logic_mgr_cfg_t *cfg, const logic_mgr_cb_t *cb)
{
        gio_shm_ra_cfg_t racfg;

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

        if (logic_mgr_build_fsm(m) != 0) {
                return -1;
        }
        if (logic_mgr_build_dispatch(m) != 0) {
                return -1;
        }
        if (logic_mgr_build_obs(m) != 0) {
                return -1;
        }

        m->engine = alloc_logic_engine();
        if (!m->engine) {
                return -1;
        }

        if (init_logic_engine(m->engine) != 0) {
                destroy_logic_engine(&m->engine);
                return -1;
        }

        m->out_map = alloc_logic_output_map();
        if (!m->out_map) {
                destroy_logic_engine(&m->engine);
                return -1;
        }

        if (init_logic_output_map(m->out_map) != 0) {
                destroy_logic_output_map(&m->out_map);
                destroy_logic_engine(&m->engine);
                return -1;
        }

        m->ra = alloc_gio_shm_ra();
        if (!m->ra) {
                destroy_logic_output_map(&m->out_map);
                destroy_logic_engine(&m->engine);
                return -1;
        }
        

        memset(&racfg, 0, sizeof(racfg));
        racfg.shm_name = "/skeleton_gio_shm";
        racfg.req_sem_name = "/skeleton_gio_req_sem";
        racfg.rsp_sem_name = "/skeleton_gio_rsp_sem";
        racfg.req_sem_init = 0U;
        racfg.rsp_sem_init = 0U;
        racfg.max_retry = 2U;
        racfg.timeout_ms = 100U;

        if (init_gio_shm_ra(m->ra, &racfg) != 0) {
                destroy_gio_shm_ra(&m->ra);
                destroy_logic_output_map(&m->out_map);
                destroy_logic_engine(&m->engine);
                return -1;
        }

        if (gio_shm_ra_connect(m->ra) != 0) {
                destroy_gio_shm_ra(&m->ra);
                destroy_logic_output_map(&m->out_map);
                destroy_logic_engine(&m->engine);
                return -1;
        }

        m->state = LOGIC_ST_IDLE;
        return 0;
}

int start_logic_mgr(logic_mgr_t *m)
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
                evt.ev = (fsm_event_t)LOGIC_MGR_EV_START;
                (void)dispatch_push(m->dispatch, &evt);
                (void)logic_mgr_process_dispatch(m, 1U, 4U);
        }

        return 0;
}

void stop_logic_mgr(logic_mgr_t *m)
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
                evt.ev = (fsm_event_t)LOGIC_MGR_EV_STOP;
                (void)dispatch_push(m->dispatch, &evt);
                (void)logic_mgr_process_dispatch(m, 1U, 4U);
        }
}

void deinit_logic_mgr(logic_mgr_t *m)
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
        if (m->ra) {
                destroy_gio_shm_ra(&m->ra);
        }
        if (m->out_map) {
                destroy_logic_output_map(&m->out_map);
        }
        if (m->engine) {
                destroy_logic_engine(&m->engine);
        }

        free(m->dispatch_qmem);
        m->dispatch_qmem = NULL;
        free(m->obs_ring_mem);
        m->obs_ring_mem = NULL;

        m->bus = NULL;
        m->state = LOGIC_ST_SHUTDOWN;
}

logic_mgr_state_t get_logic_mgr_state(const logic_mgr_t *m)
{
        if (!m) {
                return LOGIC_ST_ERR;
        }
        return m->state;
}

int logic_mgr_poll_once(logic_mgr_t *m, int timeout_ms)
{
        mgr_bus_msg_t bus_msg;

        if (!m) {
                return -EINVAL;
        }

        if (timeout_ms < 0) {
                timeout_ms = m->cfg.poll_timeout_ms;
        }

        if (m->state == LOGIC_ST_RUNNING && m->dispatch) {
                if (m->bus && mgr_bus_pop_for(m->bus, APP_MGR_ADDR_LOGIC, &bus_msg, 0) == 1) {
                        (void)logic_mgr_handle_bus_msg(m, &bus_msg);
                }

                {
                        dispatch_evt_t evt;
                        memset(&evt, 0, sizeof(evt));
                        evt.ev = (fsm_event_t)LOGIC_MGR_EV_TICK;
                        (void)dispatch_push(m->dispatch, &evt);
                }

                (void)logic_mgr_process_dispatch(m,
                                                 (uint32_t)((timeout_ms > 0) ? timeout_ms : 1),
                                                 8U);
        } else if (timeout_ms > 0) {
                usleep((useconds_t)timeout_ms * 1000U);
        }

        return 0;
}

int logic_mgr_bind_bus(logic_mgr_t *m, mgr_bus_t *bus)
{
        if (!m) {
                return -EINVAL;
        }

        m->bus = bus;
        return 0;
}

int logic_mgr_build_fsm(logic_mgr_t *m)
{
        static const fsm_trans_t tbl[] = {
                { LOGIC_ST_INIT,     LOGIC_MGR_EV_START,    LOGIC_ST_IDLE,     logic_guard_always, NULL },
                { LOGIC_ST_IDLE,     LOGIC_MGR_EV_START,    LOGIC_ST_RUNNING,  logic_guard_always, NULL },
                { LOGIC_ST_RUNNING,  LOGIC_MGR_EV_TICK,     LOGIC_ST_RUNNING,  logic_guard_always, NULL },
                { LOGIC_ST_RUNNING,  LOGIC_MGR_EV_TIMEOUT,  LOGIC_ST_ERR,      logic_guard_always, NULL },
                { LOGIC_ST_RUNNING,  LOGIC_MGR_EV_ERROR,    LOGIC_ST_ERR,      logic_guard_always, NULL },
                { LOGIC_ST_ERR,      LOGIC_MGR_EV_RECOVER,  LOGIC_ST_IDLE,     logic_guard_always, NULL },
                { LOGIC_ST_IDLE,     LOGIC_MGR_EV_STOP,     LOGIC_ST_SHUTDOWN, logic_guard_always, NULL },
                { LOGIC_ST_RUNNING,  LOGIC_MGR_EV_STOP,     LOGIC_ST_SHUTDOWN, logic_guard_always, NULL },
                { LOGIC_ST_ERR,      LOGIC_MGR_EV_STOP,     LOGIC_ST_SHUTDOWN, logic_guard_always, NULL }
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
        spec.init_state = LOGIC_ST_INIT;

        if (fsm_init(m->fsm, &spec, NULL) != 0) {
                return -1;
        }

        m->state = (logic_mgr_state_t)fsm_get_state(m->fsm);
        return 0;
}

int logic_mgr_build_dispatch(logic_mgr_t *m)
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
                m->dispatch_qcap = LOGIC_DISPATCH_QCAP;
                m->dispatch_qmem = (dispatch_qnode_t *)calloc(m->dispatch_qcap,
                                                              sizeof(dispatch_qnode_t));
                if (!m->dispatch_qmem) {
                        return -1;
                }
        }

        memset(&policy, 0, sizeof(policy));
        policy.decide = logic_dispatch_decide;

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

int logic_mgr_build_obs(logic_mgr_t *m)
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
                m->obs_ring_cap = LOGIC_OBS_RING_CAP;
                m->obs_ring_mem = (obs_evt_t *)calloc(m->obs_ring_cap, sizeof(obs_evt_t));
                if (!m->obs_ring_mem) {
                        return -1;
                }
        }

        if (obs_init(m->obs, m->obs_ring_mem, m->obs_ring_cap, logic_now_ms, NULL) != 0) {
                return -1;
        }

        return 0;
}

int logic_mgr_handle_dispatch_result(logic_mgr_t *m, const dispatch_pop_result_t *res)
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
                m->state = (logic_mgr_state_t)fsm_get_state(m->fsm);

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

void logic_mgr_begin_new_epoch(logic_mgr_t *m)
{
        if (!m) {
                return;
        }

        m->epoch++;
        if (m->obs) {
                obs_epoch_inc(m->obs);
        }
}

void logic_mgr_cancel_all(logic_mgr_t *m, uint32_t flush_q)
{
        if (!m) {
                return;
        }

        if (m->dispatch) {
                dispatch_cancel_all(m->dispatch, (flush_q ? true : false), 1U);
                dispatch_wakeup(m->dispatch);
        }
}