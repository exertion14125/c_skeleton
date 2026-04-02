#include <stdlib.h>
#include <string.h>

#include "util/fsm/fsm_observer_adapter.h"

/// @brief Adapter structure that implements the FSM observer interface and forwards calls to a user-provided observer.
struct fsm_observer_adapter_s {
        fsm_observer_t obs;
};

/// @brief Transition hook function that adapts FSM step events to the observer interface. It handles guard failures, no-ops, and real transitions, and calls the appropriate observer callbacks based on the step result.
/// @param hook_user User data pointer for the hook, expected to be a pointer to fsm_observer_adapter_t.
/// @param ctx FSM context (unused in this adapter).
/// @param before Step start state.
/// @param ev Event that triggered the step.
/// @param next_hint Next state hint (matched transition's next state, or candidate next state if guard blocked, or same as before if no-op).
/// @param after Step end state (next state if OK, otherwise same as before).
/// @param rc Result code of the FSM step (OK, NOOP, GUARD_BLOCK, or ERR).
static void adapter_tr_hook(void *hook_user, void *ctx,  fsm_state_t before, fsm_event_t ev, fsm_state_t next_hint, fsm_state_t after, fsm_step_rc_t rc)
{
        (void)ctx;
        fsm_observer_adapter_t *a = (fsm_observer_adapter_t*)hook_user;
        if (!a || !a->obs.vtbl) {
                return;
        }
        
        //=== guard fail
        if (rc == FSM_STEP_GUARD_BLOCK) {
                if (a->obs.vtbl->on_guard_fail) {
                        a->obs.vtbl->on_guard_fail(a->obs.user, (int32_t)before, (int32_t)ev, (int32_t)next_hint);
                }
        }

        //=== noop
        if (rc == FSM_STEP_NOOP) {
                if (a->obs.vtbl->on_noop) {
                        a->obs.vtbl->on_noop(a->obs.user, (int32_t)before, (int32_t)ev, (int32_t)next_hint);
                }
        }
        
        //=== enter/exit/transition only on real transition
        if (rc == FSM_STEP_OK && before != after) {
                if (a->obs.vtbl->on_exit) {
                        a->obs.vtbl->on_exit(a->obs.user, (int32_t)before, (int32_t)ev, (int32_t)after);
                }
                if (a->obs.vtbl->on_enter) {
                        a->obs.vtbl->on_enter(a->obs.user, (int32_t)after, (int32_t)ev, (int32_t)before);
                }
                if (a->obs.vtbl->on_transition) {
                        a->obs.vtbl->on_transition(a->obs.user, (int32_t)before, (int32_t)ev, (int32_t)next_hint, (int32_t)after);
                }
        }

        //=== step (always report)
        if (a->obs.vtbl->on_step) {
                a->obs.vtbl->on_step(a->obs.user, (int32_t)before, (int32_t)ev, (int32_t)next_hint, (int32_t)after, rc);
        }
}

/// @brief Allocate a new FSM observer adapter instance.
/// @return Pointer to the allocated adapter, or NULL on failure.
fsm_observer_adapter_t* alloc_fsm_observer_adapter(void)
{
        fsm_observer_adapter_t *a = (fsm_observer_adapter_t*)malloc(sizeof(*a));
        if (!a) {
                return NULL;
        }
        memset(a, 0, sizeof(*a));
        return a;
}

/// @brief Destroy the FSM observer adapter instance and free associated resources.
/// @param p Pointer to the pointer of the adapter to be destroyed. After this call, the pointer will be set to NULL.
void destroy_fsm_observer_adapter(fsm_observer_adapter_t **p)
{
        if (!p || !*p) {
                return;
        }
        free(*p);
        *p = NULL;
}

/// @brief Initialize the FSM hook with the adapter's transition hook and store a shallow copy of the observer in the adapter context. This allows the adapter to forward FSM step events to the observer callbacks.
/// @param out_hook Pointer to the FSM hook structure to be initialized with the adapter's transition hook.
/// @param ctx Pointer to the adapter context where the observer will be stored (shallow copy).
/// @param obs Pointer to the FSM observer whose callbacks will be forwarded by the adapter. The observer's vtable must be valid for the adapter to function properly.
void fsm_observer_adapter_init_hook(fsm_hook_t *out_hook, fsm_observer_adapter_t *ctx, const fsm_observer_t *obs)
{
        if (!out_hook) {
                return;
        }
        memset(out_hook, 0, sizeof(*out_hook));

        if (!ctx || !obs || !obs->vtbl) {
                return;
        }

        // shallow copy of observer (including vtbl pointer)
        ctx->obs = *obs;

        out_hook->tr_hook = adapter_tr_hook;
        out_hook->hook_user = ctx;
}