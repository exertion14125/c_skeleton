#ifndef __ENGINE_CFG_CFG_ENGINE_PRIV_H__
#define __ENGINE_CFG_CFG_ENGINE_PRIV_H__

#include "engine/cfg/cfg_engine.h"
#include "mgr/cfg/cfg_repo.h"

struct cfg_engine_s {
        cfg_repo_t repo;
        uint32_t open_count;
        uint32_t adjust_count;
        uint32_t reopen_count;
        uint32_t modify_count;
};

#endif /* __ENGINE_CFG_CFG_ENGINE_PRIV_H__ */