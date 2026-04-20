#ifndef __MGR_CFG_CFG_MGR_H__
#define __MGR_CFG_CFG_MGR_H__

#include <stdint.h>

#include "util/mgr_bus/mgr_bus.h"

typedef struct cfg_mgr_s cfg_mgr_t;

typedef enum cfg_mgr_state_e {
        CFG_ST_INIT = 0,
        CFG_ST_IDLE = 1,
        CFG_ST_RUNNING = 2,
        CFG_ST_SHUTDOWN = 3,
        CFG_ST_ERR = 4
} cfg_mgr_state_t;

typedef struct cfg_mgr_cfg_s {
        int poll_timeout_ms;
} cfg_mgr_cfg_t;

typedef struct cfg_mgr_cb_s {
        void *user;
        int  (*on_start)(void *user);
        void (*on_stop)(void *user);
} cfg_mgr_cb_t;

extern cfg_mgr_t *alloc_cfg_mgr(void);
extern void destroy_cfg_mgr(cfg_mgr_t **pm);

extern int init_cfg_mgr(cfg_mgr_t *m, const cfg_mgr_cfg_t *cfg, const cfg_mgr_cb_t *cb);
extern int start_cfg_mgr(cfg_mgr_t *m);
extern void stop_cfg_mgr(cfg_mgr_t *m);
extern void deinit_cfg_mgr(cfg_mgr_t *m);

extern cfg_mgr_state_t get_cfg_mgr_state(const cfg_mgr_t *m);
extern int cfg_mgr_poll_once(cfg_mgr_t *m, int timeout_ms);

extern int cfg_mgr_start_runloop(cfg_mgr_t *m);
extern int cfg_mgr_request_start(cfg_mgr_t *m);
extern int cfg_mgr_stop_runloop(cfg_mgr_t *m);

extern int cfg_mgr_bind_bus(cfg_mgr_t *m, mgr_bus_t *bus);

extern cfg_mgr_t *bootstrap_cfg_mgr(void);

#endif /* __MGR_CFG_CFG_MGR_H__ */