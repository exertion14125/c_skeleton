#ifndef __ENGINE_GIO_GIO_ENGINE_PRIV_H__
#define __ENGINE_GIO_GIO_ENGINE_PRIV_H__

#include "engine/gio/gio_engine.h"

struct gio_engine_s {
        uint32_t exec_count;
        uint32_t timeout_count;
        uint32_t degraded_count;

        uint64_t last_exec_ms;
        int32_t last_rc;

        uint32_t last_req_epoch;
        uint32_t last_rsp_epoch;
        uint64_t last_rsp_ts_ms;

        uint32_t last_global_err_flags;
        uint32_t last_global_err_count;
};

#endif /* __ENGINE_GIO_GIO_ENGINE_PRIV_H__ */