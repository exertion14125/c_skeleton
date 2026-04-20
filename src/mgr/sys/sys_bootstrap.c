#include <string.h>

#include "mgr/sys/sys_mgr.h"

sys_mgr_t *bootstrap_sys_mgr(void)
{
        sys_mgr_t *m;
        sys_mgr_cfg_t cfg;
        sys_mgr_cb_t cb;

        m = alloc_sys_mgr();
        if (!m) {
                return NULL;
        }

        memset(&cfg, 0, sizeof(cfg));
        memset(&cb, 0, sizeof(cb));

        cfg.poll_timeout_ms = 100;

        if (init_sys_mgr(m, &cfg, &cb) != 0) {
                destroy_sys_mgr(&m);
                return NULL;
        }

        return m;
}