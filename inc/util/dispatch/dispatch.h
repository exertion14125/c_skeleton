#ifndef __UTIL_DISPATCH_H__
#define __UTIL_DISPATCH_H__

#include <stdint.h>
#include <stdbool.h>

#include "util/dispatch/dispatch_types.h"
#include "util/dispatch/dispatch_policy.h"
#include "util/dispatch/dispatch_observer_if.h"

typedef struct dispatch_s dispatch_t;

#ifndef DISPATCH_EV_CANCEL
#define DISPATCH_EV_CANCEL ((fsm_event_t)999)
#endif

#ifndef DISPATCH_EV_CANCEL_REQ
#define DISPATCH_EV_CANCEL_REQ ((fsm_event_t)1000)
#endif


/// @brief Dispatcher queue node internal structure.
struct dispatch_qnode_s {
        dispatch_evt_t evt;
        uint64_t epoch;
};
typedef struct dispatch_qnode_s dispatch_qnode_t;

/// @brief Dispatcher cancel overflow policy
typedef enum {
        DISPATCH_CANCEL_OVF_IGN_NEW = 0,
        DISPATCH_CANCEL_OVF_OVWRITE_SLOT = 1,
        DISPATCH_CANCEL_OVF_RESET_ALL = 2,
} dispatch_cancel_ovf_policy_t;

/// @brief Dispatcher configuration
typedef struct dispatch_cfg_s {
        uint32_t drop_on_full; ///< 1: drop when full, 0: block producers
        uint32_t cancel_tab_cap; ///< req cancel table size. (0->default)
        dispatch_cancel_ovf_policy_t cancel_ovf_policy; ///< policy for cancel table overflow
        fsm_event_t ev_cancel;     ///< 0: DISPATCH_EV_CANCEL non-zero: custom event
        fsm_event_t ev_cancel_req; ///< 0: DISPATCH_EV_CANCEL_REQ non-zero: custom event
} dispatch_cfg_t;

/// @brief Dispatch pop return code
typedef enum dispatch_pop_rc_e {
        DISPATCH_POP_EMPTY = 0,
        DISPATCH_POP_OK = 1,
        DISPATCH_POP_DROP_EPOCH = 2,
        DISPATCH_POP_DROP_REQ = 3,
        DISPATCH_POP_DROP_POLICY = 4,
        DISPATCH_POP_DEFER = 5,
        DISPATCH_POP_CTRL_CANCEL_ALL = 6,
        DISPATCH_POP_CTRL_CANCEL_REQ = 7,
        DISPATCH_POP_ERR = -1
} dispatch_pop_rc_t;

/// @brief Dispatch pop result structure
typedef struct dispatch_pop_result_s {
        dispatch_pop_rc_t rc;
        dispatch_evt_t evt;
        uint64_t epoch;
        uint32_t defer_wait_ms;
        int32_t defer_reason;
} dispatch_pop_result_t;

extern dispatch_t* alloc_dispatch(void);
extern void destroy_dispatch(dispatch_t **pd);

extern int dispatch_init(dispatch_t *d, 
                        dispatch_qnode_t *qmem, 
                        uint32_t qcap,
                        const dispatch_policy_t *policy, 
                        const dispatch_observer_t *observer /* optional */,
                        const dispatch_cfg_t *cfg /* optional */);

extern void dispatch_set_observer(dispatch_t *d, const dispatch_observer_t *observer);

/// ==== producer API (thread-safe) 
extern int dispatch_push(dispatch_t *d, const dispatch_evt_t *evt); /// return: 1 pushed, 0 dropped(full), <0 err

/// consumer wait API (thread-safe)
/// - wait_timeout_ms==0 => infinite wait until non-empty
/// - wait_timeout_ms>0  => timed wait
extern uint32_t dispatch_wait(dispatch_t *d, uint32_t wait_timeout_ms);

/// consumer pop API (thread-safe)
extern dispatch_pop_rc_t dispatch_pop(dispatch_t *d, dispatch_pop_result_t *out);

extern void dispatch_wakeup(dispatch_t *d);

extern uint32_t dispatch_q_used(dispatch_t *d);
extern uint32_t dispatch_q_cap(dispatch_t *d);

extern uint32_t dispatch_get_used(const dispatch_t *d);
extern uint32_t dispatch_get_cap(const dispatch_t *d);

extern uint64_t dispatch_epoch_get(dispatch_t *d);
extern uint64_t dispatch_epoch_set(dispatch_t *d, uint64_t epoch);
extern void dispatch_cancel_all(dispatch_t *d, bool flush_q, uint32_t push_cancel_event);
extern void dispatch_cancel_req(dispatch_t *d, uint32_t req_id);

#endif /* __UTIL_DISPATCH_H__ */