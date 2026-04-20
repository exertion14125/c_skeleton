#include <string.h>

#include "mgr/red/red_mgr.h"

red_mgr_t *bootstrap_red_mgr(void)
{
        red_mgr_t *m;
        red_mgr_cfg_t cfg;
        red_mgr_cb_t cb;

        m = alloc_red_mgr();
        if (!m) {
                return NULL;
        }

        memset(&cfg, 0, sizeof(cfg));
        memset(&cb, 0, sizeof(cb));

        cfg.poll_timeout_ms = 100;

        if (init_red_mgr(m, &cfg, &cb) != 0) {
                destroy_red_mgr(&m);
                return NULL;
        }

        return m;
}