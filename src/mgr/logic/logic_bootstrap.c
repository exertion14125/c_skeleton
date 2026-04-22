#include <string.h>

#include "mgr/logic/logic_mgr.h"

logic_mgr_t *bootstrap_logic_mgr(void)
{
        logic_mgr_t *m;
        logic_mgr_cfg_t logic;
        logic_mgr_cb_t cb;

        m = alloc_logic_mgr();
        if (!m) {
                return NULL;
        }

        memset(&logic, 0, sizeof(logic));
        memset(&cb, 0, sizeof(cb));

        logic.poll_timeout_ms = 100;

        if (init_logic_mgr(m, &logic, &cb) != 0) {
                destroy_logic_mgr(&m);
                return NULL;
        }

        return m;
}