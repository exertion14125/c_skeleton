#ifndef __UI_MGR_H__
#define __UI_MGR_H__

#include <stddef.h>
#include <stdint.h>

typedef struct log_sink_s log_sink_t;   ///< log_sink_t declaration (log_sink.h include problem avoidance)

/// @brief UI limit policy enumeration.
typedef enum {
        UI_LIMIT_REJECT = 0,    ///< New UI connection rejected
        UI_LIMIT_REPLACE = 1    ///< New UI connection replaces existing one
} ui_limit_policy_t;

/// @brief UI replace mode enumeration.
typedef enum {
        UI_REPLACE_ALWAYS = 0,  ///< Always replace existing UI connection.
        UI_REPLACE_IF_STALE = 1 ///< Replace only if existing UI connection is stale
} ui_replace_mode_t;

/// @brief UI manager state enumeration. FSM states.
typedef enum {
        UI_ST_INIT = 0,
        UI_ST_IDLE = 1,
        UI_ST_HANDSHAKE = 2,
        UI_ST_CONNECTED = 3,
        UI_ST_SHUTDOWN = 4
} ui_mgr_state_t;

/// @brief UI manager configuration structure.
struct ui_mgr_cfg_s {
        char sock_path[107]; ///< UNIX domain socket path for UI connection (default APP_EXEC_NAME.ui.sock)
        int backlog;    ///< Listen backlog for UI connection
        int chmod_mode; ///< Socket file chmod mode (e.g., 0660)
        int nonblock;   ///< Set non-blocking mode for sockets

        ui_limit_policy_t limit_policy; ///< UI limit policy, see ui_limit_policy_t
        ui_replace_mode_t replace_mode;  ///< UI replace mode, see ui_replace_mode_t

        int stale_timeout_ms; ///< Stale timeout in milliseconds (0: disable)
        int poll_timeout_ms; ///< poll timeout(Using in main loop or thread).

        int rx_byte_budget; ///< Max RX byte budget per poll (0: unlimited)

        int ping_interval_ms; ///< Server ping interval in milliseconds (0: disable)
        
//         int enable_peer_cred_check; ///<SO_PEERCRED check option.
};
typedef struct ui_mgr_cfg_s ui_mgr_cfg_t;

/// @brief UI manager callback structure.
struct ui_mgr_cb_s {
        void *user;

        /// Called after handshake hello parsed successfully.
        /// Return 0 on success, negative errno on failure.
        int  (*on_attach)(void *user, int fd, uint64_t last_seen_wseq);

        /// Called when client dropped/detached.
        void (*on_detach)(void *user, int fd);
};
typedef struct ui_mgr_cb_s ui_mgr_cb_t;

typedef struct ui_mgr_s ui_mgr_t;

extern ui_mgr_t *alloc_ui_mgr(void);
extern void      destroy_ui_mgr(ui_mgr_t **mgr);

extern int  init_ui_mgr(ui_mgr_t *mgr, const ui_mgr_cfg_t *cfg, const ui_mgr_cb_t *cb);
extern int  start_ui_mgr(ui_mgr_t *mgr);
extern void stop_ui_mgr(ui_mgr_t *mgr);
extern void deinit_ui_mgr(ui_mgr_t *mgr);

extern ui_mgr_state_t get_ui_mgr_state(const ui_mgr_t *mgr);

extern int  ui_mgr_poll_once(ui_mgr_t *mgr, int timeout_ms); /// Using in main loop or thread



#endif /* __UI_MGR_H__ */