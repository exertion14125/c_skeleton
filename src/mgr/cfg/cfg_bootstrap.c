#include <string.h>

#include "mgr/cfg/cfg_mgr.h"

cfg_mgr_t *bootstrap_cfg_mgr(void)
{
        cfg_mgr_t *m;
        cfg_mgr_cfg_t cfg;
        cfg_mgr_cb_t cb;

        m = alloc_cfg_mgr();
        if (!m) {
                return NULL;
        }

        memset(&cfg, 0, sizeof(cfg));
        memset(&cb, 0, sizeof(cb));

        cfg.poll_timeout_ms = 100;

        if (init_cfg_mgr(m, &cfg, &cb) != 0) {
                destroy_cfg_mgr(&m);
                return NULL;
        }

        return m;
}