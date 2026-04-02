#ifndef __UTIL_DISPATCH_POLICY_H__
#define __UTIL_DISPATCH_POLICY_H__

#include <stdint.h>
#include "util/dispatch/dispatch_types.h"

typedef struct dispatch_s dispatch_t;

/// Policy is injected from higher layer (manager).
/// - Must be lightweight.
/// - Must NOT touch dispatcher internal locks/queue directly.

///@brief Standard DEFER reason codes (utility-level, domain-agnostic)
typedef enum {
        DISPATCH_DEFER_NONE          = 0,
        DISPATCH_DEFER_NOT_READY     = 1,  /// external condition not ready 
        DISPATCH_DEFER_RATE_LIMIT    = 2,  /// rate limiting / pacing 
        DISPATCH_DEFER_BACKPRESSURE  = 3,  /// queue near-full / watermark 
        DISPATCH_DEFER_USER          = 100 /// reserved for user extensions 
} dispatch_defer_reason_t;

/// @brief Dispatch policy structure containing function pointers for decision making, budget calculation, defer wait time, and defer reason, along with a user data pointer for context.
typedef struct dispatch_policy_s {
        dispatch_decision_t (*decide)(void *user, const dispatch_t *d, const dispatch_evt_t *evt);
        uint32_t (*budget)(void *user, const dispatch_t *d);         /// step budget per dispatch_mpsc_step() call (0 => default)
        uint32_t (*defer_wait_ms)(void *user, const dispatch_t *d, const dispatch_evt_t *evt);
        int32_t (*defer_reason)(void *user, const dispatch_t *d, const dispatch_evt_t *evt);
        void *user;
} dispatch_policy_t;

#endif /* __UTIL_DISPATCH_POLICY_H__ */