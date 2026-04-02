#ifndef __UTIL_DISPATCH_TYPES_H__
#define __UTIL_DISPATCH_TYPES_H__

#include <stdint.h>
#include "util/fsm/fsm_types.h"

/// @brief Dispatch event record
typedef struct dispatch_evt_s {
        fsm_event_t ev; ///< event to dispatch
        uint32_t req_id; ///< 0: no req/transaction, non-zero: request id
        int32_t a; ///< event-specific data
        int32_t b; ///< event-specific data

} dispatch_evt_t;

/// @brief Dispatch decision codes
typedef enum {
        DISPATCH_DECIDE_CONSUME = 0, // pop and fsm_step
        DISPATCH_DECIDE_DROP    = 1, // pop and drop
        DISPATCH_DECIDE_DEFER   = 2  // do not pop; stop processing
} dispatch_decision_t;

#endif /* __UTIL_DISPATCH_TYPES_H__ */