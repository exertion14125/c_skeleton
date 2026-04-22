#ifndef __MGR_SYS_SYS_MGR_H__
#define __MGR_SYS_SYS_MGR_H__

#include <stdint.h>

#include "util/mgr_bus/mgr_bus.h"
#include "resource/cfg/cfg_request_dto.h"
#include "resource/logic/logic_cfg_dto.h"

typedef struct sys_mgr_s sys_mgr_t;

typedef enum sys_mgr_state_e {
        SYS_ST_ERR = -1,

        SYS_ST_INIT = 0,
        SYS_ST_IDLE = 1,

        SYS_ST_BOOTSTRAP = 10,
        SYS_ST_CFG_WAIT = 11,
        SYS_ST_CFG_APPLY = 12,
        SYS_ST_RUN_PREPARE = 13,

        SYS_ST_RUN_ACTIVE = 20,
        SYS_ST_RUN_BACKUP = 21,
        SYS_ST_HOLD = 22,
        SYS_ST_FAILSAFE = 23,

        SYS_ST_SHUTDOWN = 30
} sys_mgr_state_t;

typedef enum sys_mgr_sched_policy_e {
        SYS_MGR_SCHED_BUS_FIRST = 0,
        SYS_MGR_SCHED_DISPATCH_FIRST = 1
} sys_mgr_sched_policy_t;

typedef struct sys_mgr_cfg_s {
        int poll_timeout_ms;
        uint32_t bus_budget;
        uint32_t dispatch_budget;
        sys_mgr_sched_policy_t sched_policy;
} sys_mgr_cfg_t;

typedef struct sys_mgr_cb_s {
        void *user;
        int  (*on_start)(void *user);
        void (*on_stop)(void *user);
} sys_mgr_cb_t;

extern sys_mgr_t *alloc_sys_mgr(void);
extern void destroy_sys_mgr(sys_mgr_t **pm);

extern int init_sys_mgr(sys_mgr_t *m, const sys_mgr_cfg_t *cfg, const sys_mgr_cb_t *cb);
extern int start_sys_mgr(sys_mgr_t *m);
extern void stop_sys_mgr(sys_mgr_t *m);
extern void deinit_sys_mgr(sys_mgr_t *m);

extern sys_mgr_state_t get_sys_mgr_state(const sys_mgr_t *m);
extern int sys_mgr_poll_once(sys_mgr_t *m, int timeout_ms);

extern int sys_mgr_start_runloop(sys_mgr_t *m);
extern int sys_mgr_request_start(sys_mgr_t *m);
extern int sys_mgr_stop_runloop(sys_mgr_t *m);

extern int sys_mgr_bind_bus(sys_mgr_t *m, mgr_bus_t *bus);

extern int sys_mgr_send_cfg_open(sys_mgr_t *m, const cfg_request_dto_t *req);
extern int sys_mgr_send_cfg_adjust(sys_mgr_t *m, const cfg_request_dto_t *req);
extern int sys_mgr_send_cfg_reopen(sys_mgr_t *m, const cfg_request_dto_t *req);
extern int sys_mgr_send_cfg_modify(sys_mgr_t *m, const cfg_request_dto_t *req);

extern int sys_mgr_send_gio_exec(sys_mgr_t *m, uint32_t req_id, int32_t arg);
extern int sys_mgr_send_red_eval(sys_mgr_t *m, uint32_t req_id);
extern int sys_mgr_send_logic_exec(sys_mgr_t *m, uint32_t req_id, int32_t arg);
extern int sys_mgr_send_logic_cfg(sys_mgr_t *m, const logic_cfg_dto_t *cfg);

extern sys_mgr_t *bootstrap_sys_mgr(void);

#endif /* __MGR_SYS_SYS_MGR_H__ */