#include <string.h>

#include "mgr/cfg/cfg_mgr.h"

/// @brief Bootstrap function for CFG manager. 
/// This function allocates and initializes the CFG manager, which is responsible for managing configuration data in the application. 
/// It sets up the necessary configuration parameters and callbacks for the manager to operate correctly.
/// @return A pointer to the initialized CFG manager, or NULL if initialization fails.
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