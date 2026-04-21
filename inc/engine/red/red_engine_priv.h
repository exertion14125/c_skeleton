#ifndef __ENGINE_RED_RED_ENGINE_PRIV_H__
#define __ENGINE_RED_RED_ENGINE_PRIV_H__

#include "engine/red/red_engine.h"

struct red_engine_s {
        uint32_t peer_ok_count;
        uint32_t peer_err_count;
        uint32_t timeout_count;

        uint32_t hold_flag;

        int32_t score;
        uint32_t risk_flags;

        red_policy_t policy;
        red_decision_t last_proposed_decision;
};

#endif /* __ENGINE_RED_RED_ENGINE_PRIV_H__ */