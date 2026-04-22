#ifndef __MGR_LOGIC_LOGIC_MGR_H__
#define __MGR_LOGIC_LOGIC_MGR_H__

#include <stdint.h>

#include "util/mgr_bus/mgr_bus.h"
#include "engine/logic/logic_output_map.h"

typedef struct logic_mgr_s logic_mgr_t;

typedef enum logic_mgr_state_e {
        LOGIC_ST_INIT = 0,
        LOGIC_ST_IDLE = 1,
        LOGIC_ST_RUNNING = 2,
        LOGIC_ST_SHUTDOWN = 3,
        LOGIC_ST_ERR = 4
} logic_mgr_state_t;

typedef struct logic_mgr_cfg_s {
        int poll_timeout_ms;
        logic_output_map_cfg_t out_map_cfg;
} logic_mgr_cfg_t;

typedef struct logic_mgr_cb_s {
        void *user;
        int  (*on_start)(void *user);
        void (*on_stop)(void *user);
} logic_mgr_cb_t;

extern logic_mgr_t *alloc_logic_mgr(void);
extern void destroy_logic_mgr(logic_mgr_t **pm);

extern int init_logic_mgr(logic_mgr_t *m, const logic_mgr_cfg_t *cfg, const logic_mgr_cb_t *cb);
extern int start_logic_mgr(logic_mgr_t *m);
extern void stop_logic_mgr(logic_mgr_t *m);
extern void deinit_logic_mgr(logic_mgr_t *m);

extern logic_mgr_state_t get_logic_mgr_state(const logic_mgr_t *m);
extern int logic_mgr_poll_once(logic_mgr_t *m, int timeout_ms);

extern int logic_mgr_bind_bus(logic_mgr_t *m, mgr_bus_t *bus);

#endif /* __MGR_LOGIC_LOGIC_MGR_H__ */