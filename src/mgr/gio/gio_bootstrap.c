#include <string.h>

#include "mgr/gio/gio_mgr.h"

/// @brief Bootstrap GIO manager.
/// @return Pointer to allocated and initialized GIO manager. NULL if failed.
gio_mgr_t *bootstrap_gio_mgr(void)
{
        gio_mgr_t *m;
        gio_mgr_cfg_t cfg;
        gio_mgr_cb_t cb;

        m = alloc_gio_mgr();
        if (!m) {
                return NULL;
        }

        memset(&cfg, 0, sizeof(cfg));
        memset(&cb, 0, sizeof(cb));

        cfg.poll_timeout_ms = 100;

        if (init_gio_mgr(m, &cfg, &cb) != 0) {
                destroy_gio_mgr(&m);
                return NULL;
        }

        return m;
}