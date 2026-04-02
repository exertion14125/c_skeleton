#include <string.h>
#include <stdlib.h>

#include "util/obs/obs.h"
#include "util/obs/obs_types.h"
#include "util/obs/fsm_observer_obs.h"

/// @brief Observer implementation that forwards FSM events to an obs_t instance.
struct fsm_observer_obs_s {
        obs_t *obs;
        uint32_t emit_step;
        uint32_t emit_enter_exit;
        uint32_t emit_noop;
        uint32_t emit_guard_fail;
        uint32_t emit_transition;
};

/// @brief Callback for FSM step events. It pushes a step event record into the obs instance with the before state, event, next hint, after state, and step return code.
/// @param user User data pointer, expected to be a pointer to fsm_observer_obs_t context.
/// @param before State before the step.
/// @param ev Event that triggered the step.
/// @param next_hint Hint for the next state.
/// @param after State after the step.
/// @param rc Step return code indicating the result of the step.
static void fsm_on_step(void *user, fsm_state_t before, fsm_event_t ev, fsm_state_t next_hint, fsm_state_t after, fsm_step_rc_t rc)
{
        fsm_observer_obs_t *ctx = (fsm_observer_obs_t*)user;
        if (!ctx || !ctx->obs || !ctx->emit_step) {
                return;
        }
        (void)next_hint;
        (void)after;
        (void)obs_push(ctx->obs, OBS_EVT_FSM_STEP, (int32_t)before, (int32_t)ev, (int32_t)rc);
}

/// @brief Callback for FSM transition events. It pushes a transition event record into the obs instance with the before state, event, next hint, and after state.
/// @param user User data pointer, expected to be a pointer to fsm_observer_obs_t context.
/// @param before State before the transition.
/// @param ev Event that triggered the transition.
/// @param next_hint Hint for the next state.
/// @param after State after the transition.
static void fsm_on_transition(void *user, fsm_state_t before, fsm_event_t ev, fsm_state_t next_hint, fsm_state_t after)
{
        fsm_observer_obs_t *ctx = (fsm_observer_obs_t*)user;
        if (!ctx || !ctx->obs || !ctx->emit_transition) {
                return;
        }
        (void)next_hint;
        /* 규약: epoch 증가는 "전이"에서만 */
        obs_epoch_inc(ctx->obs);
        (void)obs_push(ctx->obs, OBS_EVT_FSM_TR, (int32_t)before, (int32_t)ev, (int32_t)after);
}

/// @brief Callback for FSM exit events. It pushes an exit event record into the obs instance with the state, event, and next state.
/// @param user User data pointer, expected to be a pointer to fsm_observer_obs_t context.
/// @param st State before the exit.
/// @param ev Event that triggered the exit.
/// @param next State after the exit.
static void fsm_on_exit_cb(void *user, fsm_state_t st, fsm_event_t ev, fsm_state_t next)
{
        (void)user; (void)st; (void)ev; (void)next;
}

/// @brief Callback for FSM enter events. It pushes an enter event record into the obs instance with the state, event, and previous state.
/// @param user User data pointer, expected to be a pointer to fsm_observer_obs_t context.
/// @param st State after the enter.
/// @param ev Event that triggered the enter.
/// @param prev State before the enter.
static void fsm_on_enter_cb(void *user, fsm_state_t st, fsm_event_t ev, fsm_state_t prev)
{
        (void)user; (void)st; (void)ev; (void)prev;
}

/// @brief Callback for FSM guard failure events. It pushes a guard failure event record into the obs instance with the state, event, and next state that was blocked by the guard.
/// @param user User data pointer, expected to be a pointer to fsm_observer_obs_t context.
/// @param st State at which the guard failed.
/// @param ev Event that triggered the guard failure.
/// @param next State that was blocked by the guard.
static void fsm_on_guard_fail(void *user, fsm_state_t st, fsm_event_t ev, fsm_state_t next)
{
        fsm_observer_obs_t *ctx = (fsm_observer_obs_t*)user;
        if (!ctx || !ctx->obs || !ctx->emit_guard_fail) {
                return;
        }
        (void)obs_push(ctx->obs, OBS_EVT_FSM_GUARD_FAIL, (int32_t)st, (int32_t)ev, (int32_t)next);
}

/// @brief Callback for FSM no-op events. It pushes a no-op event record into the obs instance with the state, event, and next state hint.
/// @param user User data pointer, expected to be a pointer to fsm_observer_obs_t context.
/// @param st State at which the no-op occurred.
/// @param ev Event that triggered the no-op.
/// @param next_hint Hint for the next state that was considered for the no-op.
static void fsm_on_noop(void *user, fsm_state_t st, fsm_event_t ev, fsm_state_t next_hint)
{
        (void)next_hint;
        fsm_observer_obs_t *ctx = (fsm_observer_obs_t*)user;
        if (!ctx || !ctx->obs || !ctx->emit_noop) {
                return;
        }
        (void)obs_push(ctx->obs, OBS_EVT_FSM_NOOP, (int32_t)st, (int32_t)ev, 0);
}

/// @brief Virtual function table for the FSM observer that implements the observer callbacks to forward events to the obs instance.
static const fsm_observer_vtbl_t g_vtbl = {
        .on_step       = fsm_on_step,
        .on_transition = fsm_on_transition,
        .on_exit       = fsm_on_exit_cb,
        .on_enter      = fsm_on_enter_cb,
        .on_guard_fail = fsm_on_guard_fail,
        .on_noop       = fsm_on_noop
};

/// @brief Allocate a new fsm_observer_obs_t instance and initialize it to zero. The caller is responsible for freeing the allocated memory using destroy_fsm_observer_obs.
/// @return Pointer to the newly allocated fsm_observer_obs_t instance, or NULL if allocation fails.
fsm_observer_obs_t* alloc_fsm_observer_obs(void)
{
        fsm_observer_obs_t *ctx = (fsm_observer_obs_t*)malloc(sizeof(fsm_observer_obs_t));
        if (!ctx) {
                return NULL;
        }
        memset(ctx, 0, sizeof(*ctx));
        return ctx;
}

/// @brief Destroy a fsm_observer_obs_t instance and free its memory. After this call, the pointer will be set to NULL to prevent dangling references.
/// @param pctx Pointer to the pointer of the fsm_observer_obs_t instance to be destroyed. After this function returns, *pctx will be set to NULL.
void destroy_fsm_observer_obs(fsm_observer_obs_t **pctx)
{
        if (!pctx || !*pctx) {
                return;
        }
        free(*pctx);
        *pctx = NULL;
}

/// @brief Initialize a fsm_observer_t instance with the provided context and configuration for which events to emit.
/// @param out Pointer to the fsm_observer_t instance to be initialized.
/// @param ctx Pointer to the fsm_observer_obs_t context.
/// @param obs Pointer to the obs_t instance.
/// @param emit_step Flag to indicate whether to emit step events.
/// @param emit_enter_exit Flag to indicate whether to emit enter/exit events.
/// @param emit_noop Flag to indicate whether to emit no-op events.
/// @param emit_guard_fail Flag to indicate whether to emit guard failure events.
/// @param emit_transition Flag to indicate whether to emit transition events.
void fsm_observer_obs_init(fsm_observer_t *out, fsm_observer_obs_t *ctx, obs_t *obs, uint32_t emit_step, uint32_t emit_enter_exit, uint32_t emit_noop, uint32_t emit_guard_fail, uint32_t emit_transition)
{
        if (!out || !ctx) {
                return;
        }
        memset(ctx, 0, sizeof(*ctx));
        ctx->obs = obs;
        ctx->emit_step       = emit_step ? 1U : 0U;
        ctx->emit_enter_exit = emit_enter_exit ? 1U : 0U;
        ctx->emit_noop       = emit_noop ? 1U : 0U;
        ctx->emit_guard_fail = emit_guard_fail ? 1U : 0U;
        ctx->emit_transition = emit_transition ? 1U : 0U;

        out->vtbl = &g_vtbl;
        out->user = ctx;
}