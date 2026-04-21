#ifndef __ENGINE_LOGIC_LOGIC_ENGINE_PRIV_H__
#define __ENGINE_LOGIC_LOGIC_ENGINE_PRIV_H__

#include "engine/logic/logic_engine.h"

struct logic_engine_s {
        uint32_t exec_count;
        uint64_t last_exec_ms;
        int32_t last_rc;
};

#endif /* __ENGINE_LOGIC_LOGIC_ENGINE_PRIV_H__ */