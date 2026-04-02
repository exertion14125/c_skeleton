#ifndef __UTIL_FSM_H__
#define __UTIL_FSM_H__

#include <stdint.h>
#include "util/fsm/fsm_types.h"
#include "util/fsm/fsm_observer_if.h"

typedef struct fsm_s fsm_t;

// Domain-provided callbacks
typedef int  (*fsm_guard_fn)(void *ctx, fsm_state_t st, fsm_event_t ev, fsm_state_t next);
typedef void (*fsm_action_fn)(void *ctx, fsm_state_t st, fsm_event_t ev, fsm_state_t next);

// Optional hook (for observation/epoch) - must be lightweight
//  next_hint:
//   - FSM_STEP_OK         : equals 'after'
//   - FSM_STEP_GUARD_BLOCK: equals table's t->next (what would have happened)
//   - FSM_STEP_NOOP/ERR   : equals 'before'
typedef void (*fsm_tr_hook_fn)(void *hook_user, void *ctx, fsm_state_t before, fsm_event_t ev, fsm_state_t next_hint, fsm_state_t after, fsm_step_rc_t rc);

/// @brief FSM transition table entry
struct fsm_trans_s {
        fsm_state_t  st; ///< current state
        fsm_event_t  ev; ///< event
        fsm_state_t  next; ///< next state
        fsm_guard_fn guard;   ///< optional. If NULL, always pass
        fsm_action_fn action; ///< optional. If NULL, no action
};
typedef struct fsm_trans_s fsm_trans_t;

/// @brief FSM specification
struct fsm_spec_s {
        const fsm_trans_t *table; ///< transition table
        uint32_t table_len; ///< number of entries in table
        fsm_state_t init_state; ///< initial state
};
typedef struct fsm_spec_s fsm_spec_t;

/// @brief FSM hook callbacks
struct fsm_hook_s {
        fsm_tr_hook_fn tr_hook; ///< optional. transition hook
        void *hook_user; ///< optional. user data for hook
};
typedef struct fsm_hook_s fsm_hook_t;


extern fsm_t* alloc_fsm(void);
extern void destroy_fsm(fsm_t **pf);

extern int fsm_init(fsm_t *f, const fsm_spec_t *spec, const fsm_hook_t *hook);

extern void fsm_set_observer(fsm_t *f, const fsm_observer_t *obs);

extern fsm_state_t fsm_get_state(const fsm_t *f);

extern void fsm_force_state(fsm_t *f, fsm_state_t st); // test/restore only.

extern fsm_step_rc_t fsm_step(fsm_t *f, void *ctx, fsm_event_t ev);

#endif