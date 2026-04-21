#ifndef __MGR_GIO_GIO_MGR_H__
#define __MGR_GIO_GIO_MGR_H__

#include <stdint.h>

#include "util/mgr_bus/mgr_bus.h"

typedef struct gio_mgr_s gio_mgr_t;

typedef enum gio_mgr_state_e {
        GIO_ST_ERR = -1,

        GIO_ST_INIT = 0,
        GIO_ST_IDLE = 1,

        GIO_ST_CYCLE_WAIT = 10,
        GIO_ST_REQ_POSTED = 11,
        GIO_ST_WAIT_RESP = 12,
        GIO_ST_RX_OK = 13,
        GIO_ST_RX_TIMEOUT = 14,
        GIO_ST_DEGRADED = 15,

        GIO_ST_SHUTDOWN = 20
} gio_mgr_state_t;

typedef struct gio_mgr_cfg_s {
        int poll_timeout_ms;
} gio_mgr_cfg_t;

typedef struct gio_mgr_cb_s {
        void *user;
        int  (*on_start)(void *user);
        void (*on_stop)(void *user);
} gio_mgr_cb_t;

extern gio_mgr_t *alloc_gio_mgr(void);
extern void destroy_gio_mgr(gio_mgr_t **pm);

extern int init_gio_mgr(gio_mgr_t *m, const gio_mgr_cfg_t *cfg, const gio_mgr_cb_t *cb);
extern int start_gio_mgr(gio_mgr_t *m);
extern void stop_gio_mgr(gio_mgr_t *m);
extern void deinit_gio_mgr(gio_mgr_t *m);

extern gio_mgr_state_t get_gio_mgr_state(const gio_mgr_t *m);
extern int gio_mgr_poll_once(gio_mgr_t *m, int timeout_ms);

extern int gio_mgr_start_runloop(gio_mgr_t *m);
extern int gio_mgr_request_start(gio_mgr_t *m);
extern int gio_mgr_stop_runloop(gio_mgr_t *m);

extern int gio_mgr_bind_bus(gio_mgr_t *m, mgr_bus_t *bus);

extern gio_mgr_t *bootstrap_gio_mgr(void);

#endif /* __MGR_GIO_GIO_MGR_H__ */