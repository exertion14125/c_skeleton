#ifndef __ENGINE_SYS_SYS_ENGINE_PRIV_H__
#define __ENGINE_SYS_SYS_ENGINE_PRIV_H__

#include "engine/sys/sys_engine.h"

struct sys_engine_s {
        uint32_t cfg_rsp_count;
        uint32_t gio_rsp_count;
        uint32_t red_rsp_count;
        uint32_t gio_rx_done_count;
        uint32_t logic_rsp_count;

        uint32_t last_reason_code;
        uint32_t err_flag;

        uint32_t bootstrap_step;

        uint32_t allow_failover;
        uint32_t allow_recover;
        uint32_t hold_flag;
};

#endif /* __ENGINE_SYS_SYS_ENGINE_PRIV_H__ */