#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "util/fsm/fsm.h"

/// @brief FSM instance
struct fsm_s {
        fsm_spec_t spec_store;  ///< Store spec for internal use (in case caller's spec is on stack and goes out of scope)
        const fsm_spec_t *spec; ///< Pointer to FSM specification (points to spec_store)
        fsm_hook_t hook;        ///< FSM hooks (transition hook, etc.)
        fsm_state_t state;      ///< current state
        fsm_observer_t observer;///< optional observer (shallow copy)
};

/// @brief Find transition for given state and event
/// @param spec FSM specification
/// @param st current state
/// @param ev event
/// @return pointer to transition entry, or NULL if not found
static const fsm_trans_t* find_trans(const fsm_spec_t *spec, fsm_state_t st, fsm_event_t ev)
{
        uint32_t i;
        if (!spec || !spec->table) {
                return NULL;
        }
        for (i = 0; i < spec->table_len; i++) {
                const fsm_trans_t *trans = &spec->table[i];
                if (trans->st == st && trans->ev == ev) {
                        return trans;
                }
        }
        return NULL;
}

/// @brief Allocate FSM instance
/// @return Pointer to the allocated FSM instance, or NULL on failure
fsm_t* alloc_fsm(void)
{
        return (fsm_t*)calloc(1, sizeof(fsm_t));
}

/// @brief Destroy FSM instance
/// @param f Pointer to the FSM instance pointer
void destroy_fsm(fsm_t **pf)
{
        if (pf && *pf) {
                free(*pf);
                *pf = NULL;
        }
}

/// @brief Initialize FSM instance
/// @param f FSM instance
/// @param spec FSM specification
/// @param hook FSM hooks
/// @return 0 on success, -1 on failure
int fsm_init(fsm_t *f, const fsm_spec_t *spec, const fsm_hook_t *hook)
{
        if (!f || !spec || !spec->table || spec->table_len == 0) {
                return -1;
        }

        memset(f, 0, sizeof(*f));

        f->spec_store = *spec;
        f->spec = &f->spec_store;
        f->state = spec->init_state;

        if (hook) {
                f->hook = *hook;
        }
        return 0;
}

/// @brief Get current state of FSM
/// @param f FSM instance
/// @return current state, or (fsm_state_t)-1 if f is NULL
fsm_state_t fsm_get_state(const fsm_t *f)
{
        return f ? f->state : (fsm_state_t)-1;
}

/// @brief Set observer (shallow copy). Pass NULL to clear.
/// @param f FSM instance
/// @param obs observer to set, or NULL to clear
void fsm_set_observer(fsm_t *f, const fsm_observer_t *obs)
{
        if (!f) {
                return;
        }
        if (!obs) {
                memset(&f->observer, 0, sizeof(f->observer));
                return;
        }
        f->observer = *obs;
}

/// @brief Force set the current state of FSM
/// @param f FSM instance
/// @param st new state to set
void fsm_force_state(fsm_t *f, fsm_state_t st)
{
        if (f) {
                f->state = st;
        }
}

/// @brief Perform a step (state transition) in the FSM
/// @param f FSM instance
/// @param ctx user context passed to guard/action/hook functions
/// @param ev event to process
/// @return FSM_STEP_OK if transition happened, FSM_STEP_NOOP if no transition, FSM_STEP_GUARD_BLOCK if guard blocked, FSM_STEP_ERR on error
fsm_step_rc_t fsm_step(fsm_t *f, void *ctx, fsm_event_t ev)
{
        const fsm_trans_t *t;
        fsm_state_t before, after, next_hint;
        fsm_step_rc_t rc;

        if (!f || !f->spec) {
                 return FSM_STEP_ERR;
        }
        
        before = f->state;
        t = find_trans(f->spec, before, ev);
        if (!t) {
                rc = FSM_STEP_NOOP;
                if (f->hook.tr_hook) {
                        next_hint = before;
                        after = before;
                        f->hook.tr_hook(f->hook.hook_user, ctx, before, ev, next_hint, after, rc);
                }
                return rc;
        }
        
        next_hint = t->next; // 정확한 next 필요(guard_fail 포함)

        if (t->guard) {
                if (!t->guard(ctx, before, ev, t->next)) {
                        rc = FSM_STEP_GUARD_BLOCK;
                        if (f->hook.tr_hook) {
                                after = before;
                                f->hook.tr_hook(f->hook.hook_user, ctx, before, ev, next_hint, after, rc);
                        }
                        return rc;
                }
        }

        after = t->next;
        f->state = after;
        
        if (t->action) {
                 t->action(ctx, before, ev, after);
        }

        rc = FSM_STEP_OK;
        if (f->hook.tr_hook) {
                f->hook.tr_hook(f->hook.hook_user, ctx, before, ev, next_hint, after, rc);
        }
        return rc;
}