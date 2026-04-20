#ifndef __MGR_CONTRACT_MGR_EVENTS_H__
#define __MGR_CONTRACT_MGR_EVENTS_H__

#include <stdint.h>

#include "util/mgr_bus/mgr_bus.h"

/// @brief Common manager events.
typedef enum app_mgr_common_event_e {
        APP_MGR_EV_NONE = 0,
        APP_MGR_EV_START = 1,
        APP_MGR_EV_READY = 2,
        APP_MGR_EV_TICK = 3,
        APP_MGR_EV_TIMEOUT = 4,
        APP_MGR_EV_ERROR = 5,
        APP_MGR_EV_RETRY = 6,
        APP_MGR_EV_RECOVER_OK = 7,
        APP_MGR_EV_STOP = 8,
        APP_MGR_EV_RESET = 9
} app_mgr_common_event_t;

/// @brief SYS manager events.
typedef enum app_sys_mgr_event_e {
        APP_SYS_EV_BASE = 100,
        APP_SYS_EV_CFG_RSP,
        APP_SYS_EV_GIO_RSP,
        APP_SYS_EV_RED_RSP,
        APP_SYS_EV_UI_EVT,
        APP_SYS_EV_HEALTH_EVT
} app_sys_mgr_event_t;

/// @brief CFG manager events.
typedef enum app_cfg_mgr_event_e {
        APP_CFG_EV_BASE = 200,
        APP_CFG_EV_OPEN_REQ,
        APP_CFG_EV_ADJUST_REQ,
        APP_CFG_EV_REOPEN_REQ,
        APP_CFG_EV_MODIFY_REQ,
        APP_CFG_EV_APPLY_OK,
        APP_CFG_EV_APPLY_FAIL
} app_cfg_mgr_event_t;

/// @brief GIO manager events.
typedef enum app_gio_mgr_event_e {
        APP_GIO_EV_BASE = 300,
        APP_GIO_EV_EXEC_REQ,
        APP_GIO_EV_IPC_RX,
        APP_GIO_EV_IPC_TIMEOUT,
        APP_GIO_EV_IPC_ERROR,
        APP_GIO_EV_RETRY_EXHAUST
} app_gio_mgr_event_t;

/// @brief RED manager events.
typedef enum app_red_mgr_event_e {
        APP_RED_EV_BASE = 400,
        APP_RED_EV_EVAL_REQ,
        APP_RED_EV_HEALTH_UPDATE,
        APP_RED_EV_FAILOVER_DECIDE,
        APP_RED_EV_RECOVER_DECIDE
} app_red_mgr_event_t;

/// @brief UI manager events used in system contract.
typedef enum app_ui_mgr_event_e {
        APP_UI_EV_BASE = 500,
        APP_UI_EV_ATTACH,
        APP_UI_EV_DETACH,
        APP_UI_EV_NOTIFY_ACK
} app_ui_mgr_event_t;

/// @brief Event descriptor for debug/reporting.
typedef struct app_mgr_event_desc_s {
        int32_t ev;
        const char *name;
} app_mgr_event_desc_t;

/// @brief Event to address binding descriptor.
typedef struct app_mgr_event_binding_s {
        mgr_bus_addr_t addr;
        int32_t ev;
} app_mgr_event_binding_t;

extern const char *app_mgr_event_to_str(int32_t ev);
extern int app_mgr_event_is_common(int32_t ev);
extern int app_mgr_event_is_valid_for_addr(mgr_bus_addr_t addr, int32_t ev);
extern int app_mgr_event_is_timeout(int32_t ev);
extern int app_mgr_event_is_error(int32_t ev);
extern int app_mgr_event_is_recovery(int32_t ev);

#endif /* __MGR_CONTRACT_MGR_EVENTS_H__ */