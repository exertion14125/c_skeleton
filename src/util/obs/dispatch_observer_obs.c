#include <string.h>
#include <stdlib.h>

#include "util/obs/obs.h"
#include "util/obs/obs_types.h"
#include "util/obs/dispatch_observer_obs.h"

/// @brief Observer implementation that forwards dispatcher events to an obs_t instance. This allows recording dispatcher events in the obs event ring for later analysis.
struct dispatch_observer_obs_s {
        obs_t *obs; ///< obs instance to push events to (not owned by this observer)
        uint32_t emit_dispatch; ///< whether to emit dispatch events
        uint32_t emit_drop; ///< whether to emit drop events
        uint32_t emit_cancel; ///< whether to emit cancel events
        uint32_t emit_fsm; ///< whether to emit FSM events
};

/// @brief Map dispatch drop reason to corresponding obs event type for recording in obs.
/// @param why Reason for drop as defined in dispatch_obs_drop_t
/// @return Corresponding obs event type (obs_evt_type_t) for the drop reason
static obs_evt_type_t map_drop(dispatch_obs_drop_t why)
{
        switch (why) {
        case DISPATCH_OBS_DROP_EPOCH:  return OBS_EVT_DROP_EPOCH;
        case DISPATCH_OBS_DROP_REQ:    return OBS_EVT_DROP_REQ;
        case DISPATCH_OBS_DROP_POLICY: return OBS_EVT_DROP_POLICY;
        case DISPATCH_OBS_DROP_FULL:   return OBS_EVT_DROP_FULL;
        default:                       return OBS_EVT_DROP_POLICY;
        }
}

/// @brief Callback for dispatch events. It pushes a dispatch event record into the obs instance with the event type, request ID, and FSM return code.
/// @param user User data pointer, expected to be a pointer to dispatch_observer_obs_t context.
/// @param ev Event type of the dispatched event.
/// @param req_id Request ID associated with the dispatched event.
/// @param fsm_rc FSM return code after processing the event, which can indicate success, no-op, guard block, or error.
static void on_dispatch(void *user, int32_t ev, uint32_t req_id, int32_t fsm_rc)
{
        dispatch_observer_obs_t *ctx = (dispatch_observer_obs_t*)user;
        if (!ctx || !ctx->obs || !ctx->emit_dispatch) {
                return;
        }
        (void)obs_push(ctx->obs, OBS_EVT_DISPATCH, ev, (int32_t)req_id, fsm_rc);
}

/// @brief Callback for FSM transition events. It pushes an FSM event record into the obs instance with the before state, event, and after state. It also increments the obs epoch to indicate a new logical time point in the FSM execution.
/// @param user User data pointer, expected to be a pointer to dispatch_observer_obs_t context.
/// @param before State before the transition.
/// @param ev Event that triggered the transition.
/// @param after State after the transition.
static void on_fsm_tr(void *user, int32_t before, int32_t ev, int32_t after)
{
        dispatch_observer_obs_t *ctx = (dispatch_observer_obs_t*)user;
        if (!ctx || !ctx->obs || !ctx->emit_fsm) {
                return;
        }
        /* 규약: epoch 증가는 tr_hook(단일 지점)에서만. dispatcher는 관측만 호출 */
        obs_epoch_inc(ctx->obs);
        (void)obs_push(ctx->obs, OBS_EVT_FSM, before, ev, after);
}

/// @brief Callback for drop events. It pushes a drop event record into the obs instance with the mapped drop reason, event type, and request ID. This allows recording when and why events were dropped by the dispatcher.
/// @param user User data pointer, expected to be a pointer to dispatch_observer_obs_t context.
/// @param why Reason for the drop as defined in dispatch_obs_drop_t, which indicates why the dispatcher dropped an event (e.g., epoch mismatch, request cancellation, policy decision, or full queue).
/// @param ev Event type of the dropped event.
/// @param req_id Request ID associated with the dropped event.
static void on_drop(void *user, dispatch_obs_drop_t why, int32_t ev, uint32_t req_id)
{
        dispatch_observer_obs_t *ctx = (dispatch_observer_obs_t*)user;
        if (!ctx || !ctx->obs || !ctx->emit_drop) {
                return;
        }
        (void)obs_push(ctx->obs, map_drop(why), ev, (int32_t)req_id, 0);
}

/// @brief Callback for defer events. It pushes a defer event record into the obs instance with the event type, request ID, and a packed value containing the defer reason and suggested wait time.
///       This allows recording when events are deferred by the dispatcher and the associated reasons and timings.
/// @param user User data pointer, expected to be a pointer to dispatch_observer_obs_t context.
/// @param ev Event type of the deferred event.
/// @param req_id Request ID associated with the deferred event.
/// @param wait_ms Suggested wait time in milliseconds before the event can be retried.
/// @param reason Reason for deferring the event, which can indicate various conditions such as resource unavailability or policy constraints.
static void on_defer(void *user, int32_t ev, uint32_t req_id, uint32_t wait_ms, int32_t reason)
{
        dispatch_observer_obs_t *ctx = (dispatch_observer_obs_t*)user;
        if (!ctx || !ctx->obs) {
                return;
        }
        /* OBS_EVT_DEFER payload: (reason << 16) | (wait_ms & 0xFFFF) */
        (void)obs_push(ctx->obs, OBS_EVT_DEFER, ev, (int32_t)req_id,
                       OBS_DEFER_PACK(reason, wait_ms));
}

/// @brief Callback for cancel all events. It pushes a cancel all event record into the obs instance with the epoch split into two 32-bit values.
/// @param user User data pointer, expected to be a pointer to dispatch_observer_obs_t context.
/// @param epoch Epoch value associated with the cancel all event.
static void on_cancel_all(void *user, uint64_t epoch)
{
        dispatch_observer_obs_t *ctx = (dispatch_observer_obs_t*)user;
        if (!ctx || !ctx->obs || !ctx->emit_cancel) {
                return;
        }
        (void)obs_push(ctx->obs, OBS_EVT_CANCEL_ALL,
                      0,
                      (int32_t)(epoch & 0xFFFFFFFFu),
                      (int32_t)((epoch >> 32) & 0xFFFFFFFFu));
}

/// @brief Callback for cancel request events. It pushes a cancel request event record into the obs instance with the request ID and epoch split into two 32-bit values. This allows recording when specific requests are canceled in the dispatcher.
/// @param user User data pointer, expected to be a pointer to dispatch_observer_obs_t context.
/// @param req_id Request ID of the canceled request.
/// @param epoch Epoch value associated with the cancel request event.
static void on_cancel_req(void *user, uint32_t req_id, uint64_t epoch)
{
        dispatch_observer_obs_t *ctx = (dispatch_observer_obs_t*)user;
        if (!ctx || !ctx->obs || !ctx->emit_cancel) {
                return;
        }
        (void)obs_push(ctx->obs, OBS_EVT_CANCEL_REQ, (int32_t)req_id, (int32_t)(epoch & 0xFFFFFFFFu), (int32_t)((epoch >> 32) & 0xFFFFFFFFu));
}

/// @brief Virtual function table for the dispatch observer that implements the observer callbacks to forward events to the obs instance.
static const dispatch_observer_vtbl_t g_vtbl = {
        on_dispatch,
        on_fsm_tr,
        on_drop,
        on_defer,
        on_cancel_all,
        on_cancel_req
};

/// @brief Allocate a new dispatch_observer_obs_t instance and initialize it to zero. The caller is responsible for freeing the allocated memory using destroy_dispatch_observer_obs.
/// @return Pointer to the newly allocated dispatch_observer_obs_t instance, or NULL if allocation fails.
dispatch_observer_obs_t* alloc_dispatch_observer_obs(void)
{
        dispatch_observer_obs_t *ctx = (dispatch_observer_obs_t*)malloc(sizeof(dispatch_observer_obs_t));
        if (!ctx) {
                return NULL;
        }
        memset(ctx, 0, sizeof(*ctx));
        return ctx;
}

/// @brief Destroy a dispatch_observer_obs_t instance and free its memory. After this call, the pointer will be set to NULL to prevent dangling references.
/// @param pctx Pointer to the pointer of the dispatch_observer_obs_t instance to be destroyed. After this function returns, *pctx will be set to NULL.
void destroy_dispatch_observer_obs(dispatch_observer_obs_t **pctx)
{
        if (!pctx || !*pctx) {
                return;
        }
        free(*pctx);
        *pctx = NULL;
}

/// @brief Initialize a dispatch_observer_t instance with the provided context and configuration for which events to emit.
/// @param out Pointer to the dispatch_observer_t instance to be initialized.
/// @param ctx Pointer to the dispatch_observer_obs_t context to be used by the observer.
/// @param obs Pointer to the obs instance where events will be pushed.
/// @param emit_dispatch Flag indicating whether to emit dispatch events.
/// @param emit_drop Flag indicating whether to emit drop events.
/// @param emit_cancel Flag indicating whether to emit cancel events.
/// @param emit_fsm Flag indicating whether to emit FSM events.
void dispatch_observer_obs_init(dispatch_observer_t *out, dispatch_observer_obs_t *ctx, obs_t *obs, uint32_t emit_dispatch, uint32_t emit_drop, uint32_t emit_cancel, uint32_t emit_fsm)
{
        if (!out || !ctx) {
                return;
        }
        memset(ctx, 0, sizeof(*ctx));
        ctx->obs = obs;
        ctx->emit_dispatch = emit_dispatch ? 1U : 0U;
        ctx->emit_drop = emit_drop ? 1U : 0U;
        ctx->emit_cancel = emit_cancel ? 1U : 0U;
        ctx->emit_fsm = emit_fsm ? 1U : 0U;

        out->vtbl = &g_vtbl;
        out->user = ctx;
}