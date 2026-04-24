#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include "mgr/sys/sys_mgr_priv.h"
#include "mgr/contract/mgr_addrs.h"
#include "mgr/contract/mgr_msg_codes.h"

#include "ra/cfg/cfg_request_ra.h"
#include "ra/cfg/cfg_shm_ra.h"
#include "ra/logic/logic_cfg_ra.h"
#include "resource/cfg/dto/cfg_request_dto.h"
#include "resource/cfg/dto/cfg_export_dto.h"
#include "resource/logic/logic_cfg_dto.h"
#include "resource/gio/gio_snapshot_dto.h"

#define SYS_DISPATCH_QCAP    64U
#define SYS_OBS_RING_CAP     128U

static uint64_t sys_now_ms(void *user)
{
        struct timespec ts;
        (void)user;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static int sys_guard_always(void *ctx, fsm_state_t st, fsm_event_t ev, fsm_state_t next)
{
        (void)ctx;
        (void)st;
        (void)ev;
        (void)next;
        return 1;
}

static dispatch_decision_t sys_dispatch_decide(void *user, const dispatch_t *d, const dispatch_evt_t *evt)
{
        (void)user;
        (void)d;
        (void)evt;
        return DISPATCH_DECIDE_CONSUME;
}

static int sys_mgr_process_dispatch(sys_mgr_t *m, uint32_t wait_ms, uint32_t budget)
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
                (void)sys_mgr_handle_dispatch_result(m, &res);
                n++;
        }

        return 0;
}

static sys_eng_input_kind_t sys_mgr_red_msg_to_input_kind(uint16_t code)
{
        switch (code) {
        case APP_MGR_MSG_RED_PROPOSE_FAILOVER_EVT:
                return SYS_ENG_IN_RED_PROPOSE_FAILOVER;

        case APP_MGR_MSG_RED_PROPOSE_RECOVER_EVT:
                return SYS_ENG_IN_RED_PROPOSE_RECOVER;

        case APP_MGR_MSG_RED_PROPOSE_HOLD_EVT:
                return SYS_ENG_IN_RED_PROPOSE_HOLD;

        case APP_MGR_MSG_RED_DECISION_RSP:
        default:
                return SYS_ENG_IN_RED_RSP;
        }
}

static sys_eng_input_kind_t sys_mgr_gio_msg_to_input_kind(uint16_t code)
{
        switch (code) {
        case APP_MGR_MSG_GIO_RX_DONE_EVT:
                return SYS_ENG_IN_GIO_RX_DONE;

        case APP_MGR_MSG_GIO_EXEC_RSP:
        case APP_MGR_MSG_GIO_TIMEOUT_EVT:
        case APP_MGR_MSG_GIO_DEGRADED_EVT:
        default:
                return SYS_ENG_IN_GIO_RSP;
        }
}

static int sys_mgr_send_cfg_request(sys_mgr_t *m,
                                    uint16_t code,
                                    const cfg_request_dto_t *req)
{
        if (!m || !m->bus || !m->cfg_request_ra || !req) {
                return -EINVAL;
        }

        if (cfg_request_ra_write(m->cfg_request_ra, req) != 0) {
            return -1;
        }

        return mgr_bus_send(m->bus,
                            APP_MGR_ADDR_SYS,
                            APP_MGR_ADDR_CFG,
                            code,
                            (int32_t)req->version,
                            0,
                            sys_now_ms(NULL));
}

static int sys_mgr_apply_logic_cfg_from_cfg_export(sys_mgr_t *m)
{
        cfg_export_dto_t cfgexp;
        logic_cfg_dto_t logiccfg;
        uint32_t route_count;

        if (!m || !m->cfg_shm_ra) {
                return -EINVAL;
        }

        memset(&cfgexp, 0, sizeof(cfgexp));
        if (cfg_shm_ra_read_effective_cfg(m->cfg_shm_ra, &cfgexp) != 0) {
                return -1;
        }

        memset(&logiccfg, 0, sizeof(logiccfg));
        logiccfg.version = cfgexp.cfg_generation;
        logiccfg.valid = 1U;
        logiccfg.ts_ms = sys_now_ms(NULL);

        route_count = (cfgexp.total_channel_count >= 2U) ? 2U : cfgexp.total_channel_count;
        logiccfg.out_map_cfg.route_count = route_count;

        if (route_count > 0U) {
                logiccfg.out_map_cfg.routes[0].valid = 1U;
                logiccfg.out_map_cfg.routes[0].out_card_no = 0U;
                logiccfg.out_map_cfg.routes[0].out_card_type = GIO_CARD_TYPE_DO;
                logiccfg.out_map_cfg.routes[0].out_value_index = 0U;
                logiccfg.out_map_cfg.routes[0].out_ch = 0U;
        }

        if (route_count > 1U) {
                logiccfg.out_map_cfg.routes[1].valid = 1U;
                logiccfg.out_map_cfg.routes[1].out_card_no = 0U;
                logiccfg.out_map_cfg.routes[1].out_card_type = GIO_CARD_TYPE_DO;
                logiccfg.out_map_cfg.routes[1].out_value_index = 1U;
                logiccfg.out_map_cfg.routes[1].out_ch = 1U;
        }

        return sys_mgr_send_logic_cfg(m, &logiccfg);
}

int sys_mgr_send_logic_exec(sys_mgr_t *m, uint32_t req_id, int32_t arg)
{
        if (!m || !m->bus) {
                return -EINVAL;
        }

        return mgr_bus_send(m->bus,
                            APP_MGR_ADDR_SYS,
                            APP_MGR_ADDR_LOGIC,
                            APP_MGR_MSG_SYS_LOGIC_EXEC_REQ,
                            (int32_t)req_id,
                            arg,
                            sys_now_ms(NULL));
}

int sys_mgr_send_logic_cfg(sys_mgr_t *m, const logic_cfg_dto_t *cfg)
{
        if (!m || !m->bus || !m->logic_cfg_ra || !cfg) {
                return -EINVAL;
        }

        if (logic_cfg_ra_write(m->logic_cfg_ra, cfg) != 0) {
                return -1;
        }

        return mgr_bus_send(m->bus,
                            APP_MGR_ADDR_SYS,
                            APP_MGR_ADDR_LOGIC,
                            APP_MGR_MSG_SYS_LOGIC_CFG_REQ,
                            (int32_t)cfg->version,
                            0,
                            sys_now_ms(NULL));
}

static int sys_mgr_execute_action(sys_mgr_t *m, const sys_eng_output_t *out)
{
        if (!m || !out) {
                return -EINVAL;
        }

        switch (out->action) {
        case SYS_ENG_ACT_SEND_CFG_OPEN: {
                cfg_request_dto_t req;

                memset(&req, 0, sizeof(req));
                req.version = out->req_id;
                req.valid = 1U;
                req.ts_ms = sys_now_ms(NULL);
                req.req_kind = CFG_REQ_KIND_OPEN;

                return sys_mgr_send_cfg_open(m, &req);
        }

        case SYS_ENG_ACT_SEND_CFG_ADJUST: {
                cfg_request_dto_t req;

                memset(&req, 0, sizeof(req));
                req.version = out->req_id;
                req.valid = 1U;
                req.ts_ms = sys_now_ms(NULL);
                req.req_kind = CFG_REQ_KIND_ADJUST;
                req.arg0 = out->value0;
                req.arg1 = out->value1;

                return sys_mgr_send_cfg_adjust(m, &req);
        }

#ifdef SYS_ENG_ACT_SEND_CFG_REOPEN
        case SYS_ENG_ACT_SEND_CFG_REOPEN: {
                cfg_request_dto_t req;

                memset(&req, 0, sizeof(req));
                req.version = out->req_id;
                req.valid = 1U;
                req.ts_ms = sys_now_ms(NULL);
                req.req_kind = CFG_REQ_KIND_REOPEN;

                return sys_mgr_send_cfg_reopen(m, &req);
        }
#endif

#ifdef SYS_ENG_ACT_SEND_CFG_MODIFY
        case SYS_ENG_ACT_SEND_CFG_MODIFY: {
                cfg_request_dto_t req;

                memset(&req, 0, sizeof(req));
                req.version = out->req_id;
                req.valid = 1U;
                req.ts_ms = sys_now_ms(NULL);
                req.req_kind = CFG_REQ_KIND_MODIFY;

                req.modify.out_card_no = (uint32_t)out->value0;
                req.modify.out_card_type = GIO_CARD_TYPE_DO;
                req.modify.out_ch0 = (uint32_t)out->value1;
                req.modify.out_ch1 = (uint32_t)out->value1 + 1U;

                return sys_mgr_send_cfg_modify(m, &req);
        }
#endif

        case SYS_ENG_ACT_SEND_GIO_EXEC:
                return sys_mgr_send_gio_exec(m, out->req_id, out->value0);

        case SYS_ENG_ACT_SEND_RED_EVAL:
                return sys_mgr_send_red_eval(m, out->req_id);

        case SYS_ENG_ACT_APPROVE_FAILOVER:
                m->state = SYS_ST_RUN_ACTIVE;
                if (m->obs) {
                        (void)obs_push(m->obs,
                                       OBS_EVT_DISPATCH,
                                       9001,
                                       (int32_t)out->req_id,
                                       1);
                }
                return 0;

        case SYS_ENG_ACT_REJECT_FAILOVER:
                if (m->obs) {
                        (void)obs_push(m->obs,
                                       OBS_EVT_DISPATCH,
                                       9002,
                                       (int32_t)out->req_id,
                                       (int32_t)out->reason_code);
                }
                return 0;

        case SYS_ENG_ACT_APPROVE_RECOVER:
                m->state = SYS_ST_RUN_BACKUP;
                if (m->obs) {
                        (void)obs_push(m->obs,
                                       OBS_EVT_DISPATCH,
                                       9003,
                                       (int32_t)out->req_id,
                                       1);
                }
                return 0;

        case SYS_ENG_ACT_REJECT_RECOVER:
                if (m->obs) {
                        (void)obs_push(m->obs,
                                       OBS_EVT_DISPATCH,
                                       9004,
                                       (int32_t)out->req_id,
                                       (int32_t)out->reason_code);
                }
                return 0;

        case SYS_ENG_ACT_ENTER_HOLD:
                m->state = SYS_ST_HOLD;
                if (m->obs) {
                        (void)obs_push(m->obs,
                                       OBS_EVT_DISPATCH,
                                       9005,
                                       (int32_t)out->req_id,
                                       1);
                }
                return 0;

        case SYS_ENG_ACT_CFG_ACCEPTED:
                m->state = SYS_ST_CFG_APPLY;
                return 0;

        case SYS_ENG_ACT_GIO_ACCEPTED:
                m->state = SYS_ST_RUN_PREPARE;
                return 0;

        case SYS_ENG_ACT_RED_ACCEPTED:
        case SYS_ENG_ACT_KEEP:
                return 0;

        case SYS_ENG_ACT_LOGIC_READY:
                if (m->obs) {
                        (void)obs_push(m->obs,
                                       OBS_EVT_DISPATCH,
                                       9010,
                                       (int32_t)out->req_id,
                                       1);
                }
                return sys_mgr_send_logic_exec(m, out->req_id, out->value0);

        case SYS_ENG_ACT_ENTER_ERR:
                m->state = SYS_ST_FAILSAFE;
                return 0;

        case SYS_ENG_ACT_NONE:
        default:
                return 0;
        }
}

static int sys_mgr_apply_engine(sys_mgr_t *m,
                                sys_eng_input_kind_t kind,
                                const mgr_bus_msg_t *msg)
{
        sys_eng_input_t in;
        sys_eng_output_t out;
        int rc;

        if (!m || !m->engine || !msg) {
                return -EINVAL;
        }

        memset(&in, 0, sizeof(in));
        memset(&out, 0, sizeof(out));

        in.kind = kind;
        in.msg_code = msg->code;
        in.req_id = (uint32_t)msg->a;
        in.arg0 = msg->a;
        in.arg1 = msg->b;
        in.now_ms = sys_now_ms(NULL);

        rc = sys_engine_apply(m->engine, &in, &out);
        if (rc != 0) {
                return rc;
        }

        if (m->obs) {
                (void)obs_push(m->obs,
                               OBS_EVT_DISPATCH,
                               (int32_t)msg->code,
                               msg->a,
                               msg->b);
        }

        return sys_mgr_execute_action(m, &out);
}

static int sys_mgr_on_cfg_rsp(sys_mgr_t *m, const mgr_bus_msg_t *msg)
{
        int rc;

        if (!m || !msg) {
                return -EINVAL;
        }

        rc = sys_mgr_apply_engine(m, SYS_ENG_IN_CFG_RSP, msg);

        if (msg->b == 0) {
                (void)sys_mgr_apply_logic_cfg_from_cfg_export(m);
        }

        return rc;
}

static int sys_mgr_on_gio_rsp(sys_mgr_t *m, const mgr_bus_msg_t *msg)
{
        sys_eng_input_kind_t kind;

        if (!m || !msg) {
                return -EINVAL;
        }

        kind = sys_mgr_gio_msg_to_input_kind(msg->code);
        return sys_mgr_apply_engine(m, kind, msg);
}

static int sys_mgr_on_red_rsp(sys_mgr_t *m, const mgr_bus_msg_t *msg)
{
        sys_eng_input_kind_t kind;

        if (!m || !msg) {
                return -EINVAL;
        }

        kind = sys_mgr_red_msg_to_input_kind(msg->code);
        return sys_mgr_apply_engine(m, kind, msg);
}

static int sys_mgr_on_logic_rsp(sys_mgr_t *m, const mgr_bus_msg_t *msg)
{
        if (!m || !msg) {
                return -EINVAL;
        }

        return sys_mgr_apply_engine(m, SYS_ENG_IN_LOGIC_RSP, msg);
}

static int sys_mgr_route_bus_msg(sys_mgr_t *m, const mgr_bus_msg_t *msg)
{
        if (!m || !msg) {
                return -EINVAL;
        }

        switch (msg->code) {
        case APP_MGR_MSG_CFG_OPEN_RSP:
        case APP_MGR_MSG_CFG_ADJUST_RSP:
        case APP_MGR_MSG_CFG_REOPEN_RSP:
        case APP_MGR_MSG_CFG_MODIFY_RSP:
                return sys_mgr_on_cfg_rsp(m, msg);

        case APP_MGR_MSG_GIO_EXEC_RSP:
        case APP_MGR_MSG_GIO_TIMEOUT_EVT:
        case APP_MGR_MSG_GIO_DEGRADED_EVT:
        case APP_MGR_MSG_GIO_RX_DONE_EVT:
                return sys_mgr_on_gio_rsp(m, msg);

        case APP_MGR_MSG_RED_DECISION_RSP:
        case APP_MGR_MSG_RED_PROPOSE_FAILOVER_EVT:
        case APP_MGR_MSG_RED_PROPOSE_RECOVER_EVT:
        case APP_MGR_MSG_RED_PROPOSE_HOLD_EVT:
                return sys_mgr_on_red_rsp(m, msg);

        case APP_MGR_MSG_LOGIC_EXEC_RSP:
        case APP_MGR_MSG_LOGIC_CFG_RSP:
                return sys_mgr_on_logic_rsp(m, msg);

        default:
                return 0;
        }
}

static int sys_mgr_is_pollable_state(sys_mgr_state_t st)
{
        switch (st) {
        case SYS_ST_BOOTSTRAP:
        case SYS_ST_CFG_WAIT:
        case SYS_ST_CFG_APPLY:
        case SYS_ST_RUN_PREPARE:
        case SYS_ST_RUN_ACTIVE:
        case SYS_ST_RUN_BACKUP:
        case SYS_ST_HOLD:
        case SYS_ST_FAILSAFE:
                return 1;

        case SYS_ST_INIT:
        case SYS_ST_IDLE:
        case SYS_ST_SHUTDOWN:
        case SYS_ST_ERR:
        default:
                return 0;
        }
}

static int sys_mgr_run_bootstrap(sys_mgr_t *m)
{
        sys_eng_input_t in;
        sys_eng_output_t out;
        int rc;
        uint32_t guard = 0U;

        if (!m || !m->engine) {
                return -EINVAL;
        }

        while (guard < 8U) {
                memset(&in, 0, sizeof(in));
                memset(&out, 0, sizeof(out));

                in.kind = SYS_ENG_IN_BOOTSTRAP;
                in.now_ms = sys_now_ms(NULL);

                rc = sys_engine_apply(m->engine, &in, &out);
                if (rc != 0) {
                        return rc;
                }

                if (out.action == SYS_ENG_ACT_KEEP ||
                    out.action == SYS_ENG_ACT_NONE) {
                        break;
                }

                rc = sys_mgr_execute_action(m, &out);
                if (rc != 0) {
                        return rc;
                }

                guard++;
        }

        return 0;
}

static fsm_event_t sys_mgr_tick_event_for_state(sys_mgr_state_t st)
{
        switch (st) {
        case SYS_ST_BOOTSTRAP:
        case SYS_ST_CFG_WAIT:
        case SYS_ST_CFG_APPLY:
        case SYS_ST_RUN_PREPARE:
        case SYS_ST_RUN_ACTIVE:
        case SYS_ST_RUN_BACKUP:
        case SYS_ST_HOLD:
        case SYS_ST_FAILSAFE:
                return (fsm_event_t)SYS_MGR_EV_TICK;

        default:
                return (fsm_event_t)SYS_MGR_EV_NONE;
        }
}

static int sys_mgr_drain_bus(sys_mgr_t *m, uint32_t budget)
{
        mgr_bus_msg_t bus_msg;
        uint32_t n = 0U;

        if (!m || !m->bus) {
                return 0;
        }

        while (n < budget) {
                if (mgr_bus_pop_for(m->bus, APP_MGR_ADDR_SYS, &bus_msg, 0) != 1) {
                        break;
                }
                (void)sys_mgr_route_bus_msg(m, &bus_msg);
                n++;
        }

        return (int)n;
}

static void sys_mgr_inject_tick(sys_mgr_t *m)
{
        fsm_event_t tick_ev;
        dispatch_evt_t evt;

        if (!m || !m->dispatch) {
                return;
        }

        tick_ev = sys_mgr_tick_event_for_state(m->state);
        if (tick_ev == (fsm_event_t)SYS_MGR_EV_NONE) {
                return;
        }

        memset(&evt, 0, sizeof(evt));
        evt.ev = tick_ev;
        (void)dispatch_push(m->dispatch, &evt);
}

sys_mgr_t *alloc_sys_mgr(void)
{
        sys_mgr_t *m = (sys_mgr_t *)calloc(1, sizeof(*m));
        if (!m) {
                return NULL;
        }

        m->state = SYS_ST_INIT;
        return m;
}

void destroy_sys_mgr(sys_mgr_t **pm)
{
        sys_mgr_t *m;

        if (!pm || !*pm) {
                return;
        }

        m = *pm;
        *pm = NULL;

        deinit_sys_mgr(m);
        free(m);
}

int init_sys_mgr(sys_mgr_t *m, const sys_mgr_cfg_t *cfg, const sys_mgr_cb_t *cb)
{
        cfg_shm_ra_cfg_t cfg_shm_cfg;

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
        if (m->cfg.bus_budget == 0) {
                m->cfg.bus_budget = 8U;
        }
        if (m->cfg.dispatch_budget == 0) {
                m->cfg.dispatch_budget = 8U;
        }
        if (m->cfg.sched_policy != SYS_MGR_SCHED_BUS_FIRST &&
            m->cfg.sched_policy != SYS_MGR_SCHED_DISPATCH_FIRST) {
                m->cfg.sched_policy = SYS_MGR_SCHED_BUS_FIRST;
        }

        if (sys_mgr_build_fsm(m) != 0) {
                return -1;
        }
        if (sys_mgr_build_dispatch(m) != 0) {
                return -1;
        }
        if (sys_mgr_build_obs(m) != 0) {
                return -1;
        }

        m->engine = alloc_sys_engine();
        if (!m->engine) {
                return -1;
        }

        if (init_sys_engine(m->engine) != 0) {
                destroy_sys_engine(&m->engine);
                return -1;
        }

        m->cfg_request_ra = alloc_cfg_request_ra();
        if (!m->cfg_request_ra) {
                destroy_sys_engine(&m->engine);
                return -1;
        }

        if (init_cfg_request_ra(m->cfg_request_ra) != 0) {
                destroy_cfg_request_ra(&m->cfg_request_ra);
                destroy_sys_engine(&m->engine);
                return -1;
        }

        m->cfg_shm_ra = alloc_cfg_shm_ra();
        if (!m->cfg_shm_ra) {
                destroy_cfg_request_ra(&m->cfg_request_ra);
                destroy_sys_engine(&m->engine);
                return -1;
        }

        memset(&cfg_shm_cfg, 0, sizeof(cfg_shm_cfg));
        cfg_shm_cfg.shm_name = "/skeleton_gio_shm";

        if (init_cfg_shm_ra(m->cfg_shm_ra, &cfg_shm_cfg) != 0 ||
            cfg_shm_ra_connect(m->cfg_shm_ra) != 0) {
                destroy_cfg_shm_ra(&m->cfg_shm_ra);
                destroy_cfg_request_ra(&m->cfg_request_ra);
                destroy_sys_engine(&m->engine);
                return -1;
        }

        m->logic_cfg_ra = alloc_logic_cfg_ra();
        if (!m->logic_cfg_ra) {
                destroy_cfg_shm_ra(&m->cfg_shm_ra);
                destroy_cfg_request_ra(&m->cfg_request_ra);
                destroy_sys_engine(&m->engine);
                return -1;
        }

        if (init_logic_cfg_ra(m->logic_cfg_ra) != 0) {
                destroy_logic_cfg_ra(&m->logic_cfg_ra);
                destroy_cfg_shm_ra(&m->cfg_shm_ra);
                destroy_cfg_request_ra(&m->cfg_request_ra);
                destroy_sys_engine(&m->engine);
                return -1;
        }

        m->state = SYS_ST_IDLE;
        return 0;
}

int start_sys_mgr(sys_mgr_t *m)
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
                evt.ev = (fsm_event_t)SYS_MGR_EV_START;
                (void)dispatch_push(m->dispatch, &evt);
                (void)sys_mgr_process_dispatch(m, 1U, 4U);
        }

        rc = sys_mgr_run_bootstrap(m);
        if (rc != 0) {
                return rc;
        }

        return 0;
}

void stop_sys_mgr(sys_mgr_t *m)
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
                evt.ev = (fsm_event_t)SYS_MGR_EV_STOP;
                (void)dispatch_push(m->dispatch, &evt);
                (void)sys_mgr_process_dispatch(m, 1U, 4U);
        }
}

void deinit_sys_mgr(sys_mgr_t *m)
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
        if (m->logic_cfg_ra) {
                destroy_logic_cfg_ra(&m->logic_cfg_ra);
        }
        if (m->cfg_shm_ra) {
                destroy_cfg_shm_ra(&m->cfg_shm_ra);
        }
        if (m->cfg_request_ra) {
                destroy_cfg_request_ra(&m->cfg_request_ra);
        }
        if (m->engine) {
                destroy_sys_engine(&m->engine);
        }

        free(m->dispatch_qmem);
        m->dispatch_qmem = NULL;
        free(m->obs_ring_mem);
        m->obs_ring_mem = NULL;

        m->bus = NULL;
        m->state = SYS_ST_SHUTDOWN;
}

sys_mgr_state_t get_sys_mgr_state(const sys_mgr_t *m)
{
        if (!m) {
                return SYS_ST_ERR;
        }
        return m->state;
}

int sys_mgr_poll_once(sys_mgr_t *m, int timeout_ms)
{
        uint32_t bus_budget;
        uint32_t dispatch_budget;

        if (!m) {
                return -EINVAL;
        }

        if (timeout_ms < 0) {
                timeout_ms = m->cfg.poll_timeout_ms;
        }

        if (!sys_mgr_is_pollable_state(m->state) || !m->dispatch) {
                if (timeout_ms > 0) {
                        usleep((useconds_t)timeout_ms * 1000U);
                }
                return 0;
        }

        bus_budget = m->cfg.bus_budget;
        dispatch_budget = m->cfg.dispatch_budget;

        if (m->cfg.sched_policy == SYS_MGR_SCHED_DISPATCH_FIRST) {
                sys_mgr_inject_tick(m);
                (void)sys_mgr_process_dispatch(m,
                                               (uint32_t)((timeout_ms > 0) ? timeout_ms : 1),
                                               dispatch_budget);
                (void)sys_mgr_drain_bus(m, bus_budget);
        } else {
                (void)sys_mgr_drain_bus(m, bus_budget);
                sys_mgr_inject_tick(m);
                (void)sys_mgr_process_dispatch(m,
                                               (uint32_t)((timeout_ms > 0) ? timeout_ms : 1),
                                               dispatch_budget);
        }

        return 0;
}

int sys_mgr_bind_bus(sys_mgr_t *m, mgr_bus_t *bus)
{
        if (!m) {
                return -EINVAL;
        }

        m->bus = bus;
        return 0;
}

int sys_mgr_send_cfg_open(sys_mgr_t *m, const cfg_request_dto_t *req)
{
        return sys_mgr_send_cfg_request(m,
                                        APP_MGR_MSG_SYS_CFG_OPEN_REQ,
                                        req);
}

int sys_mgr_send_cfg_adjust(sys_mgr_t *m, const cfg_request_dto_t *req)
{
        return sys_mgr_send_cfg_request(m,
                                        APP_MGR_MSG_SYS_CFG_ADJUST_REQ,
                                        req);
}

int sys_mgr_send_cfg_reopen(sys_mgr_t *m, const cfg_request_dto_t *req)
{
        return sys_mgr_send_cfg_request(m,
                                        APP_MGR_MSG_SYS_CFG_REOPEN_REQ,
                                        req);
}

int sys_mgr_send_cfg_modify(sys_mgr_t *m, const cfg_request_dto_t *req)
{
        return sys_mgr_send_cfg_request(m,
                                        APP_MGR_MSG_SYS_CFG_MODIFY_REQ,
                                        req);
}

int sys_mgr_send_gio_exec(sys_mgr_t *m, uint32_t req_id, int32_t arg)
{
        if (!m || !m->bus) {
                return -EINVAL;
        }

        return mgr_bus_send(m->bus,
                            APP_MGR_ADDR_SYS,
                            APP_MGR_ADDR_GIO,
                            APP_MGR_MSG_SYS_GIO_EXEC_REQ,
                            (int32_t)req_id,
                            arg,
                            sys_now_ms(NULL));
}

int sys_mgr_send_red_eval(sys_mgr_t *m, uint32_t req_id)
{
        if (!m || !m->bus) {
                return -EINVAL;
        }

        return mgr_bus_send(m->bus,
                            APP_MGR_ADDR_SYS,
                            APP_MGR_ADDR_RED,
                            APP_MGR_MSG_SYS_RED_EVAL_REQ,
                            (int32_t)req_id,
                            0,
                            sys_now_ms(NULL));
}

int sys_mgr_build_fsm(sys_mgr_t *m)
{
        static const fsm_trans_t tbl[] = {
                { SYS_ST_INIT,        SYS_MGR_EV_START,    SYS_ST_IDLE,         sys_guard_always, NULL },
                { SYS_ST_IDLE,        SYS_MGR_EV_START,    SYS_ST_BOOTSTRAP,    sys_guard_always, NULL },

                { SYS_ST_BOOTSTRAP,   SYS_MGR_EV_TICK,     SYS_ST_BOOTSTRAP,    sys_guard_always, NULL },
                { SYS_ST_CFG_WAIT,    SYS_MGR_EV_TICK,     SYS_ST_CFG_WAIT,     sys_guard_always, NULL },
                { SYS_ST_CFG_APPLY,   SYS_MGR_EV_TICK,     SYS_ST_CFG_APPLY,    sys_guard_always, NULL },
                { SYS_ST_RUN_PREPARE, SYS_MGR_EV_TICK,     SYS_ST_RUN_PREPARE,  sys_guard_always, NULL },

                { SYS_ST_RUN_ACTIVE,  SYS_MGR_EV_TICK,     SYS_ST_RUN_ACTIVE,   sys_guard_always, NULL },
                { SYS_ST_RUN_BACKUP,  SYS_MGR_EV_TICK,     SYS_ST_RUN_BACKUP,   sys_guard_always, NULL },
                { SYS_ST_HOLD,        SYS_MGR_EV_TICK,     SYS_ST_HOLD,         sys_guard_always, NULL },
                { SYS_ST_FAILSAFE,    SYS_MGR_EV_TICK,     SYS_ST_FAILSAFE,     sys_guard_always, NULL },

                { SYS_ST_BOOTSTRAP,   SYS_MGR_EV_ERROR,    SYS_ST_FAILSAFE,     sys_guard_always, NULL },
                { SYS_ST_CFG_WAIT,    SYS_MGR_EV_ERROR,    SYS_ST_FAILSAFE,     sys_guard_always, NULL },
                { SYS_ST_CFG_APPLY,   SYS_MGR_EV_ERROR,    SYS_ST_FAILSAFE,     sys_guard_always, NULL },
                { SYS_ST_RUN_PREPARE, SYS_MGR_EV_ERROR,    SYS_ST_FAILSAFE,     sys_guard_always, NULL },
                { SYS_ST_RUN_ACTIVE,  SYS_MGR_EV_ERROR,    SYS_ST_FAILSAFE,     sys_guard_always, NULL },
                { SYS_ST_RUN_BACKUP,  SYS_MGR_EV_ERROR,    SYS_ST_FAILSAFE,     sys_guard_always, NULL },
                { SYS_ST_HOLD,        SYS_MGR_EV_ERROR,    SYS_ST_FAILSAFE,     sys_guard_always, NULL },

                { SYS_ST_FAILSAFE,    SYS_MGR_EV_RECOVER,  SYS_ST_IDLE,         sys_guard_always, NULL },

                { SYS_ST_IDLE,        SYS_MGR_EV_STOP,     SYS_ST_SHUTDOWN,     sys_guard_always, NULL },
                { SYS_ST_BOOTSTRAP,   SYS_MGR_EV_STOP,     SYS_ST_SHUTDOWN,     sys_guard_always, NULL },
                { SYS_ST_CFG_WAIT,    SYS_MGR_EV_STOP,     SYS_ST_SHUTDOWN,     sys_guard_always, NULL },
                { SYS_ST_CFG_APPLY,   SYS_MGR_EV_STOP,     SYS_ST_SHUTDOWN,     sys_guard_always, NULL },
                { SYS_ST_RUN_PREPARE, SYS_MGR_EV_STOP,     SYS_ST_SHUTDOWN,     sys_guard_always, NULL },
                { SYS_ST_RUN_ACTIVE,  SYS_MGR_EV_STOP,     SYS_ST_SHUTDOWN,     sys_guard_always, NULL },
                { SYS_ST_RUN_BACKUP,  SYS_MGR_EV_STOP,     SYS_ST_SHUTDOWN,     sys_guard_always, NULL },
                { SYS_ST_HOLD,        SYS_MGR_EV_STOP,     SYS_ST_SHUTDOWN,     sys_guard_always, NULL },
                { SYS_ST_FAILSAFE,    SYS_MGR_EV_STOP,     SYS_ST_SHUTDOWN,     sys_guard_always, NULL }
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
        spec.init_state = SYS_ST_INIT;

        if (fsm_init(m->fsm, &spec, NULL) != 0) {
                return -1;
        }

        m->state = (sys_mgr_state_t)fsm_get_state(m->fsm);
        return 0;
}

int sys_mgr_build_dispatch(sys_mgr_t *m)
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
                m->dispatch_qcap = SYS_DISPATCH_QCAP;
                m->dispatch_qmem = (dispatch_qnode_t *)calloc(m->dispatch_qcap,
                                                              sizeof(dispatch_qnode_t));
                if (!m->dispatch_qmem) {
                        return -1;
                }
        }

        memset(&policy, 0, sizeof(policy));
        policy.decide = sys_dispatch_decide;

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

int sys_mgr_build_obs(sys_mgr_t *m)
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
                m->obs_ring_cap = SYS_OBS_RING_CAP;
                m->obs_ring_mem = (obs_evt_t *)calloc(m->obs_ring_cap, sizeof(obs_evt_t));
                if (!m->obs_ring_mem) {
                        return -1;
                }
        }

        if (obs_init(m->obs, m->obs_ring_mem, m->obs_ring_cap, sys_now_ms, NULL) != 0) {
                return -1;
        }

        return 0;
}

int sys_mgr_handle_dispatch_result(sys_mgr_t *m, const dispatch_pop_result_t *res)
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
                m->state = (sys_mgr_state_t)fsm_get_state(m->fsm);

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

void sys_mgr_begin_new_epoch(sys_mgr_t *m)
{
        if (!m) {
                return;
        }

        m->epoch++;
        if (m->obs) {
                obs_epoch_inc(m->obs);
        }
}

void sys_mgr_cancel_all(sys_mgr_t *m, uint32_t flush_q)
{
        if (!m) {
                return;
        }

        if (m->dispatch) {
                dispatch_cancel_all(m->dispatch, (flush_q ? true : false), 1U);
                dispatch_wakeup(m->dispatch);
        }
}