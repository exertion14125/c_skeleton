#ifndef __MGR_RED_RED_MGR_H__
#define __MGR_RED_RED_MGR_H__

#include <stdint.h>

#include "util/mgr_bus/mgr_bus.h"

typedef struct red_mgr_s red_mgr_t;

typedef enum red_mgr_state_e {
        RED_ST_INIT = 0,
        RED_ST_IDLE = 1,
        RED_ST_RUNNING = 2,
        RED_ST_SHUTDOWN = 3,
        RED_ST_ERR = 4
} red_mgr_state_t;

typedef struct red_mgr_cfg_s {
        int poll_timeout_ms;
} red_mgr_cfg_t;

typedef struct red_mgr_cb_s {
        void *user;
        int  (*on_start)(void *user);
        void (*on_stop)(void *user);
} red_mgr_cb_t;

extern red_mgr_t *alloc_red_mgr(void);
extern void destroy_red_mgr(red_mgr_t **pm);

extern int init_red_mgr(red_mgr_t *m, const red_mgr_cfg_t *cfg, const red_mgr_cb_t *cb);
extern int start_red_mgr(red_mgr_t *m);
extern void stop_red_mgr(red_mgr_t *m);
extern void deinit_red_mgr(red_mgr_t *m);

extern red_mgr_state_t get_red_mgr_state(const red_mgr_t *m);
extern int red_mgr_poll_once(red_mgr_t *m, int timeout_ms);

extern int red_mgr_start_runloop(red_mgr_t *m);
extern int red_mgr_request_start(red_mgr_t *m);
extern int red_mgr_stop_runloop(red_mgr_t *m);

extern int red_mgr_bind_bus(red_mgr_t *m, mgr_bus_t *bus);

extern red_mgr_t *bootstrap_red_mgr(void);

#endif /* __MGR_RED_RED_MGR_H__ */