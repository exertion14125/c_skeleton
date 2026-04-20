#ifndef __MGR_CONTRACT_MGR_MSG_CODES_H__
#define __MGR_CONTRACT_MGR_MSG_CODES_H__

#include <stdint.h>

/// @brief Manager message domains used for code range grouping.
typedef enum app_mgr_msg_domain_e {
        APP_MGR_MSG_DOM_NONE = 0,
        APP_MGR_MSG_DOM_SYS  = 1,
        APP_MGR_MSG_DOM_CFG  = 2,
        APP_MGR_MSG_DOM_GIO  = 3,
        APP_MGR_MSG_DOM_RED  = 4,
        APP_MGR_MSG_DOM_UI   = 5,
        APP_MGR_MSG_DOM_HEALTH = 6
} app_mgr_msg_domain_t;

/// @brief Message code list for inter-manager requests/responses/events.
typedef enum app_mgr_msg_code_e {
        APP_MGR_MSG_NONE = 0,

        APP_MGR_MSG_SYS_CFG_OPEN_REQ      = 0x1101,
        APP_MGR_MSG_SYS_CFG_ADJUST_REQ    = 0x1102,
        APP_MGR_MSG_SYS_CFG_REOPEN_REQ    = 0x1103,
        APP_MGR_MSG_SYS_CFG_MODIFY_REQ    = 0x1104,

        APP_MGR_MSG_SYS_GIO_EXEC_REQ      = 0x1201,

        APP_MGR_MSG_SYS_RED_EVAL_REQ      = 0x1301,

        APP_MGR_MSG_CFG_OPEN_RSP          = 0x2101,
        APP_MGR_MSG_CFG_ADJUST_RSP        = 0x2102,
        APP_MGR_MSG_CFG_REOPEN_RSP        = 0x2103,
        APP_MGR_MSG_CFG_MODIFY_RSP        = 0x2104,

        APP_MGR_MSG_GIO_EXEC_RSP          = 0x2201,
        APP_MGR_MSG_GIO_TIMEOUT_EVT       = 0x2202,
        APP_MGR_MSG_GIO_DEGRADED_EVT      = 0x2203,

        APP_MGR_MSG_RED_DECISION_RSP      = 0x2301,

        APP_MGR_MSG_UI_NOTIFY_EVT         = 0x2401,

        APP_MGR_MSG_HEALTH_REPORT_EVT     = 0x3001
} app_mgr_msg_code_t;

/// @brief Optional message flag bits.
typedef enum app_mgr_msg_flags_e {
        APP_MGR_MSG_F_NONE    = 0,
        APP_MGR_MSG_F_ACK_REQ = (1u << 0),
        APP_MGR_MSG_F_URGENT  = (1u << 1),
        APP_MGR_MSG_F_RETRY   = (1u << 2)
} app_mgr_msg_flags_t;

/// @brief Message code descriptor for debugging/reporting.
typedef struct app_mgr_msg_desc_s {
        uint16_t code;
        uint16_t domain;
        const char *name;
} app_mgr_msg_desc_t;

/// @brief Domain code range descriptor.
typedef struct app_mgr_msg_code_range_s {
        uint16_t domain;
        uint16_t begin_code;
        uint16_t end_code;
} app_mgr_msg_code_range_t;

extern const char *app_mgr_msg_code_to_str(uint16_t code);
extern const char *app_mgr_msg_domain_to_str(uint16_t domain);
extern int app_mgr_msg_code_is_valid(uint16_t code);
extern int app_mgr_msg_code_get_domain(uint16_t code, uint16_t *out_domain);
extern int app_mgr_msg_code_is_request(uint16_t code);
extern int app_mgr_msg_code_is_response(uint16_t code);
extern int app_mgr_msg_code_is_event(uint16_t code);
extern int app_mgr_msg_code_is_health(uint16_t code);

#endif /* __MGR_CONTRACT_MGR_MSG_CODES_H__ */