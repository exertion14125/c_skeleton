#ifndef __UTIL_DISPATCH_OBSERVER_IF_H__
#define __UTIL_DISPATCH_OBSERVER_IF_H__

#include <stdint.h>

/// Dispatch observer interface (Utility-only)
/// - dispatch core is not include util/observer.
/// - observer only observes (no behavior modification).

/// @brief Reasons for dropping a request in the dispatch system. 
typedef enum {
        DISPATCH_OBS_DROP_EPOCH  = 1,
        DISPATCH_OBS_DROP_REQ    = 2,
        DISPATCH_OBS_DROP_POLICY = 3,
        DISPATCH_OBS_DROP_FULL   = 4
} dispatch_obs_drop_t;

/// @brief Reasons for deferring a request in the dispatch system.
typedef enum {
        DISPATCH_OBS_DEFER_NONE         = 0,
        DISPATCH_OBS_DEFER_NOT_READY    = 1,
        DISPATCH_OBS_DEFER_RATE_LIMIT   = 2,
        DISPATCH_OBS_DEFER_BACKPRESSURE = 3,
        DISPATCH_OBS_DEFER_USER         = 100
} dispatch_obs_defer_reason_t;

/// @brief Virtual table structure for dispatch observer, containing function pointers for various observer callbacks.
typedef struct dispatch_observer_vtbl_s {
        void (*on_dispatch)(void *user, int32_t ev, uint32_t req_id, int32_t fsm_rc);
        void (*on_fsm_tr)(void *user, int32_t before, int32_t ev, int32_t after);
        void (*on_drop)(void *user, dispatch_obs_drop_t why, int32_t ev, uint32_t req_id);
        void (*on_defer)(void *user, int32_t ev, uint32_t req_id, uint32_t wait_ms, int32_t reason);
        void (*on_cancel_all)(void *user, uint64_t epoch);
        void (*on_cancel_req)(void *user, uint32_t req_id, uint64_t epoch);
} dispatch_observer_vtbl_t;

/// @brief Dispatch observer structure containing a pointer to the virtual table and a user-defined data pointer.
typedef struct dispatch_observer_s {
        const dispatch_observer_vtbl_t *vtbl;
        void *user;
} dispatch_observer_t;

#endif /* __UTIL_DISPATCH_OBSERVER_IF_H__ */