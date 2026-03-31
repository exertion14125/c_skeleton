#include <stdio.h>
#include <string.h>
#include <error.h>

#include "skeleton_const.h"

#include "mgr/ui/ui_mgr_bootstrap.h"

/// @brief UI log attach callback function.
/// @param user user pointer.
/// @param fd UDS socket fd.
/// @param last_seen_wseq last seen write sequence.
/// @return 0 on success, negative errno on failure.
static int cb_ui_on_attach(void *user, int fd, uint64_t last_seen_wseq)
{
        (void)user;
        return attach_log_system_ui(fd, last_seen_wseq);
}

/// @brief UI log detach callback function.
/// @param user user pointer.
/// @param fd UDS socket fd.
static void cb_ui_on_detach(void *user, int fd)
{
        (void)user;
        (void)detach_log_system_ui(fd);
}

/// @brief Bootstrap UI manager.
/// @param ui_mgr Pointer to UI manager pointer.
ui_mgr_t* bootstrap_ui_mgr(void)
{
        ui_mgr_t* ui_mgr = NULL;
        if ((ui_mgr = alloc_ui_mgr())) {
                ui_mgr_cfg_t ucfg;
                memset(&ucfg, 0, sizeof(ucfg));
                snprintf(ucfg.sock_path, sizeof(ucfg.sock_path), "%s" APP_EXEC_NAME "_ui.sock", DEF_LOG_UI_UDS_PATH);
                ucfg.backlog = 4;
                ucfg.chmod_mode = 0660;
                ucfg.nonblock = 1;
                ucfg.limit_policy = UI_LIMIT_REPLACE;
                ucfg.replace_mode = UI_REPLACE_ALWAYS;

                ucfg.poll_timeout_ms = 100;
                ucfg.rx_byte_budget = 1024;

                ucfg.ping_interval_ms = 0; // Disable ping.
                ucfg.stale_timeout_ms = 0; // Disable stale timeout.

                ui_mgr_cb_t cb;
                memset(&cb, 0, sizeof(cb));
                cb.user = NULL;
                cb.on_attach = cb_ui_on_attach;
                cb.on_detach = cb_ui_on_detach;

                if (init_ui_mgr(ui_mgr, &ucfg, &cb) == 0) {
                        return ui_mgr;
                } else {
                        destroy_ui_mgr(&ui_mgr);
                        ui_mgr = NULL;
                }
        }
        return NULL;
}