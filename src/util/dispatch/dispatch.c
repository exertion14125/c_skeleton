#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>

#include "util/dispatch/dispatch.h"

#define DISPATCH_CANCEL_TAB_DEFAULT 128 ///< default cancel table size

/// @brief Cancel entry structure
struct cancel_ent_s {
        uint32_t req_id; ///< request id
        uint64_t epoch; ///< epoch when canceled
        uint8_t  valid; ///< 1: valid, 0: invalid
};
typedef struct cancel_ent_s cancel_ent_t;

/// @brief Dispatcher structure
struct dispatch_s {
        dispatch_qnode_t *q; ///< ring buffer
        uint32_t cap; ///< capacity of the ring buffer
        uint32_t head; ///< index of the next element to pop
        uint32_t tail; ///< index of the next element to push

        pthread_mutex_t mtx; ///< mutex for thread safety
        pthread_cond_t cv_nonempty; ///< condition variable for non-empty queue
        pthread_cond_t cv_nonfull; ///< condition variable for non-full queue

        clockid_t cv_clock; ///< clock id used by cv_nonempty timedwait
        
        uint32_t defer_active; ///< 1: last step ended with DEFER (queue not popped)
        uint64_t defer_until_ms; ///< 0: indefinite wait until wakeup, else absolute ms

        uint64_t wake_seq; ///< wakeup latch sequence (monotonic). increment on dispatch_wakeup()
        uint64_t defer_seq; ///< snapshot of wake_seq when defer became active (latched wake)

        uint32_t drop_on_full; ///< 1: drop when full, 0: block producers

        dispatch_cancel_ovf_policy_t cancel_ovf_policy; ///< policy for cancel table overflow

        fsm_event_t ev_cancel;     ///< injected cancel-all event id
        fsm_event_t ev_cancel_req; ///< injected cancel-req event id

        uint64_t active_epoch; ///< active epoch for cancellations

        cancel_ent_t *cancel_tab; ///< cancellation table
        uint32_t cancel_tab_cap; ///< capacity of the cancellation table

        dispatch_policy_t policy; ///< copied
        dispatch_observer_t observer; ///< observe-only callbacks (optional)
};

/// @brief Emit drop event to observer
/// @param d Pointer to the dispatcher
/// @param why Reason for drop
/// @param evt Pointer to the dropped event
static void emit_drop(dispatch_t *d, dispatch_obs_drop_t why, const dispatch_evt_t *evt)
{
        if (!d || !evt) {
                return;
        }
        if (d->observer.vtbl && d->observer.vtbl->on_drop) {
                d->observer.vtbl->on_drop(d->observer.user, why,
                        (int32_t)evt->ev, (uint32_t)evt->req_id);
        }
}

/// @brief Emit defer event to observer
/// @param d Pointer to the dispatcher
/// @param epoch Epoch of the defer
static void emit_cancel_all(dispatch_t *d, uint64_t epoch)
{
        if (!d) {
                return;
        }
        if (d->observer.vtbl && d->observer.vtbl->on_cancel_all) {
                d->observer.vtbl->on_cancel_all(d->observer.user, epoch);
        }
}

/// @brief Emit cancel_req event to observer
/// @param d Pointer to the dispatcher
/// @param req_id Request ID being canceled
/// @param epoch Epoch of the cancel_req
static void emit_cancel_req(dispatch_t *d, uint32_t req_id, uint64_t epoch)
{
        if (!d) {
                return;
        }
        if (d->observer.vtbl && d->observer.vtbl->on_cancel_req) {
                d->observer.vtbl->on_cancel_req(d->observer.user, req_id, epoch);
        }
}

/// @brief Emit dispatch event to observer
/// @param d Pointer to the dispatcher
/// @param evt Pointer to the dispatched event
/// @param rc Return code after processing the event
static void emit_dispatch(dispatch_t *d, const dispatch_evt_t *evt, int32_t rc)
{
        if (!d || !evt) {
                return;
        }
        if (d->observer.vtbl && d->observer.vtbl->on_dispatch) {
                d->observer.vtbl->on_dispatch(d->observer.user,
                        (int32_t)evt->ev, (uint32_t)evt->req_id, rc);
        }
}

/// @brief Emit defer event to observer
/// @param d Pointer to the dispatcher
/// @param ev Event id being deferred
/// @param req_id Request id (0 if none)
/// @param wait_ms 0: indefinite wait, >0: suggested wait time (ms)
/// @param reason Defer reason code (dispatch_obs_defer_reason_t or user-defined)
static void emit_defer(dispatch_t *d, int32_t ev, uint32_t req_id, uint32_t wait_ms, int32_t reason)
{
        if (!d) {
                return;
        }
        if (d->observer.vtbl && d->observer.vtbl->on_defer) {
                d->observer.vtbl->on_defer(d->observer.user, ev, req_id, wait_ms, reason);
        }
}

/// @brief Get current time in milliseconds for the specified clock
/// @param clk Clock ID
/// @return Current time in milliseconds
static uint64_t now_ms_clk(clockid_t clk)
{
        struct timespec ts;
        clock_gettime(clk, &ts);
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/// @brief 32-bit hash function
/// @param x input value
/// @return hashed value
static uint32_t h32(uint32_t x)
{
        x ^= x >> 16;
        x *= 0x7feb352dU;
        x ^= x >> 15;
        x *= 0x846ca68bU;
        x ^= x >> 16;
        return x;
}

/// @brief Make absolute timespec from now + timeout_ms
/// @param ts Pointer to timespec to fill
/// @param timeout_ms Timeout in milliseconds
static void make_abs_timespec_ms(struct timespec *ts, uint32_t timeout_ms, clockid_t clk)
{
        struct timespec now;
        clock_gettime(clk, &now);

        ts->tv_sec  = now.tv_sec + (time_t)(timeout_ms / 1000U);
        ts->tv_nsec = now.tv_nsec + (long)((timeout_ms % 1000U) * 1000000UL);

        if (ts->tv_nsec >= 1000000000L) {
                ts->tv_sec += 1;
                ts->tv_nsec -= 1000000000L;
        }
}

/// @brief Get number of used slots in the ring buffer
/// @param d Pointer to the dispatcher structure
/// @return Number of used slots
static uint32_t q_used_n(dispatch_t *d)
{
        if (d->tail >= d->head) {
                return (d->tail - d->head);
        }
        return (d->cap - (d->head - d->tail));
}

/// @brief Check if the ring buffer is empty
/// @param d Pointer to the dispatcher structure
/// @return 1 if empty, 0 otherwise
static uint32_t q_is_empty(dispatch_t *d)
{
        return (d->head == d->tail);
}

/// @brief Check if the ring buffer is full
/// @param d Pointer to the dispatcher structure
/// @return 1 if full, 0 otherwise
static uint32_t q_is_full(dispatch_t *d)
{
        uint32_t next = d->tail + 1;
        if (next >= d->cap) {
                next = 0;
        }
        return (next == d->head);
}

/// @brief Peek head event from the dispatcher (locked, does not pop)
/// @param d Pointer to the dispatcher structure
/// @param out Pointer to store the peeked event
/// @param out_idx Pointer to store the head index
/// @return 1 if an event exists, 0 if the queue is empty
static int peek_one_locked(dispatch_t *d, dispatch_qnode_t *out, uint32_t *out_idx)
{
        if (q_is_empty(d)) {
                return 0;
        }
        if (out) {
                *out = d->q[d->head];
        }
        if (out_idx) {
                *out_idx = d->head;
        }
        return 1;
}

/// @brief Commit pop for the previously peeked head (locked)
/// @param d Pointer to the dispatcher structure
/// @param expect_idx expect_idx Expected head index from peek
/// @return 1 if popped, 0 if head changed (not popped)
static int pop_commit_locked(dispatch_t *d, uint32_t expect_idx)
{
        if (q_is_empty(d)) {
                return 0;
        }

        if (d->head != expect_idx) {
                return 0;
        }
        d->head++;
        if (d->head >= d->cap) {
                d->head = 0;
        }

        pthread_cond_signal(&d->cv_nonfull);
        return 1;
}

/// @brief Check if a request is canceled (locked)
/// @param d Pointer to the dispatcher structure
/// @param req_id Request ID to check
/// @return 1 if canceled, 0 otherwise
static int is_req_canceled_locked(dispatch_t *d, uint32_t req_id)
{
        uint32_t i, idx;

        if (req_id == 0 || !d->cancel_tab || d->cancel_tab_cap == 0) {
                return 0;
        }

        idx = h32(req_id) % d->cancel_tab_cap;
        for (i = 0; i < d->cancel_tab_cap; ++i) {
                cancel_ent_t *e = &d->cancel_tab[(idx + i) % d->cancel_tab_cap];
                if (!e->valid) {
                        return 0;
                }
                if (e->req_id == req_id) {
                        return (e->epoch == d->active_epoch) ? 1 : 0;
                }
        }
        return 0;
}

/// @brief Insert a cancellation entry into the cancel table (locked)
/// @param d Pointer to the dispatcher structure
/// @param req_id Request ID to cancel
/// @param epoch Epoch at which cancellation occurs
/// @return 1 if inserted, 0 if not inserted (table full and policy is IGNORE_NEW)
static int cancel_tab_insert_locked(dispatch_t *d, uint32_t req_id, uint64_t epoch)
{
        uint32_t i, idx;

        if (!d->cancel_tab || d->cancel_tab_cap == 0) {
                return 0;
        }

        idx = h32(req_id) % d->cancel_tab_cap;
        for (i = 0; i < d->cancel_tab_cap; ++i) {
                cancel_ent_t *e = &d->cancel_tab[(idx + i) % d->cancel_tab_cap];
                if (!e->valid || e->req_id == req_id) {
                        e->valid = 1;
                        e->req_id = req_id;
                        e->epoch = epoch;
                        return 1;
                }
        }

        if (d->cancel_ovf_policy == DISPATCH_CANCEL_OVF_IGN_NEW) {
                return 0;
        }

        if (d->cancel_ovf_policy == DISPATCH_CANCEL_OVF_RESET_ALL) {
                memset(d->cancel_tab, 0, sizeof(cancel_ent_t) * d->cancel_tab_cap);
                /* insert at hashed slot */
                d->cancel_tab[idx].valid = 1;
                d->cancel_tab[idx].req_id = req_id;
                d->cancel_tab[idx].epoch = epoch;
                return 1;
        }
        
        d->cancel_tab[idx].valid = 1;
        d->cancel_tab[idx].req_id = req_id;
        d->cancel_tab[idx].epoch = epoch;
        return 1;
}

/// @brief Allocate a dispatcher
/// @return Pointer to the allocated dispatcher, or NULL on failure
dispatch_t* alloc_dispatch(void)
{
        dispatch_t *d = (dispatch_t *)malloc(sizeof(dispatch_t));
        if (d) {
                memset(d, 0, sizeof(*d));
        }
        return d;
}

/// @brief Destroy the dispatcher
/// @param d Pointer to the dispatcher structure pointer
void destroy_dispatch(dispatch_t **pd)
{
        if (!pd || !*pd) {
                return;
        }

        dispatch_t *d = *pd;
        pthread_mutex_destroy(&d->mtx);
        pthread_cond_destroy(&d->cv_nonempty);
        pthread_cond_destroy(&d->cv_nonfull);

        if (d->cancel_tab) {
                free(d->cancel_tab);
                d->cancel_tab = NULL;
        }

        memset(d, 0, sizeof(*d));
        free(d);
        *pd = NULL;
}

/// @brief Set the observer for the dispatcher
/// @param d Pointer to the dispatcher
/// @param observer Pointer to the observer structure (NULL to clear)
void dispatch_set_observer(dispatch_t *d, const dispatch_observer_t *observer)
{
        if (!d) {
                return;
        }
        pthread_mutex_lock(&d->mtx);
        if (observer) {
                d->observer = *observer; /* shallow copy */
        } else {
                memset(&d->observer, 0, sizeof(d->observer));
        }
        pthread_mutex_unlock(&d->mtx);
}

/// @brief Initialize the dispatcher
/// @param d Pointer to the dispatcher structure
/// @param qmem Memory for the event queue
/// @param qcap Capacity of the event queue
/// @param policy Dispatch policy
/// @param observer Dispatch observer callbacks (optional)
/// @param cfg Dispatcher configuration (optional)
/// @return 0 on success, -1 on failure
int dispatch_init(dispatch_t *d, 
                dispatch_qnode_t *qmem, 
                uint32_t qcap,
                const dispatch_policy_t *policy, 
                const dispatch_observer_t *observer /* optional */,
                const dispatch_cfg_t *cfg /* optional */)
{
        if (!d || !qmem || qcap < 2 || !policy || !policy->decide) {
                return -1;
        }

        memset(d, 0, sizeof(*d));
        d->q = qmem;
        d->cap = qcap;
        d->policy = *policy;

        if (observer) {
                d->observer = *observer; /* shallow copy */
        }

        d->drop_on_full = 1;
        d->cancel_ovf_policy = DISPATCH_CANCEL_OVF_OVWRITE_SLOT;
        d->ev_cancel = DISPATCH_EV_CANCEL;
        d->ev_cancel_req = DISPATCH_EV_CANCEL_REQ;

        if (cfg) {
                d->drop_on_full = (cfg->drop_on_full ? 1U : 0U);
                d->cancel_ovf_policy = cfg->cancel_ovf_policy;
                if (cfg->ev_cancel != 0) {
                        d->ev_cancel = cfg->ev_cancel;
                }
                if (cfg->ev_cancel_req != 0) {
                        d->ev_cancel_req = cfg->ev_cancel_req;
                }
        }

        d->active_epoch = 1;
        d->defer_active = 0;
        d->defer_until_ms = 0;
        d->wake_seq = 1;
        d->defer_seq = d->wake_seq;

        if (cfg && cfg->cancel_tab_cap > 0) {
                d->cancel_tab_cap = cfg->cancel_tab_cap;
        } else {
                d->cancel_tab_cap = DISPATCH_CANCEL_TAB_DEFAULT;
        }

        d->cancel_tab = (cancel_ent_t *)calloc(d->cancel_tab_cap, sizeof(cancel_ent_t));
        if (!d->cancel_tab) {
                return -1;
        }

        pthread_mutex_init(&d->mtx, 0);
        
        d->cv_clock = CLOCK_REALTIME;
#if defined(CLOCK_MONOTONIC) && defined(_POSIX_CLOCK_SELECTION)
        {
                //Prefer MONOTONIC for timed waits: immune to wall-clock adjustments */
                pthread_condattr_t attr;
                if (pthread_condattr_init(&attr) == 0) {
                        if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) == 0) {
                                d->cv_clock = CLOCK_MONOTONIC;
                        }
                        (void)pthread_cond_init(&d->cv_nonempty, &attr);
                        (void)pthread_cond_init(&d->cv_nonfull, &attr);
                        (void)pthread_condattr_destroy(&attr);
                } else {
                        pthread_cond_init(&d->cv_nonempty, 0);
                        pthread_cond_init(&d->cv_nonfull, 0);
                }
        }
#else
        pthread_cond_init(&d->cv_nonempty, 0);
        pthread_cond_init(&d->cv_nonfull, 0);
#endif
        return 0;
}

/// @brief Push an event into the dispatcher
/// @param d Pointer to the dispatcher structure
/// @param evt Pointer to the event to push
/// @return 1 if pushed, 0 if dropped (full), -1 on error
int dispatch_push(dispatch_t *d, const dispatch_evt_t *evt)
{
        if (!d || !evt) {
                return -1;
        }

        pthread_mutex_lock(&d->mtx);

        while (q_is_full(d)) {
                if (d->drop_on_full) {
                        emit_drop(d, DISPATCH_OBS_DROP_FULL, evt);
                        pthread_mutex_unlock(&d->mtx);
                        return 0;
                }
                pthread_cond_wait(&d->cv_nonfull, &d->mtx);
        }

        d->q[d->tail].evt = *evt;
        d->q[d->tail].epoch = d->active_epoch;

        d->tail++;
        if (d->tail >= d->cap) {
                d->tail = 0;
        }

        pthread_cond_signal(&d->cv_nonempty);
        pthread_mutex_unlock(&d->mtx);
        return 1;
}

/// @brief Wait for events and perform a dispatch step
/// @param d Pointer to the dispatcher structure
/// @param wait_timeout_ms Timeout in milliseconds (0 for infinite wait)
/// @return Number of events processed
uint32_t dispatch_wait(dispatch_t *d, uint32_t wait_timeout_ms)
{
        int r = 0;
        int timedout = 0;
        uint64_t start_ms = 0;
        uint64_t deadline_ms = 0;

        if (!d) {
                return 0U;
        }

        /// Strict total-timeout semantics:
        ///  wait_timeout_ms is the TOTAL budget for this call
        ///  we must not exceed it even if we loop multiple times
        /// Note: if wait_timeout_ms==0 -> infinite.
        start_ms = now_ms_clk(d->cv_clock);
        if (wait_timeout_ms != 0) {
                /* overflow-safe deadline (strict total timeout semantics) */
                uint64_t add = (uint64_t)wait_timeout_ms;
                if (UINT64_MAX - start_ms < add) {
                        deadline_ms = UINT64_MAX;
                } else {
                        deadline_ms = start_ms + add;
                }
        }
        pthread_mutex_lock(&d->mtx);
        while(1) {
                uint32_t remaining_ms = 0;
                if (wait_timeout_ms != 0) {
                        uint64_t now = now_ms_clk(d->cv_clock);
                        if (now >= deadline_ms) {
                                timedout = 1;
                                break;
                        }
                        remaining_ms = (uint32_t)(deadline_ms - now);
                        if (remaining_ms == 0) {
                                timedout = 1;
                                break;
                        }
                }
                if (q_is_empty(d)) {
                        if (wait_timeout_ms == 0) {
                                r = pthread_cond_wait(&d->cv_nonempty, &d->mtx);
                                (void)r;
                        } else {
                                struct timespec ts;
                                make_abs_timespec_ms(&ts, remaining_ms, d->cv_clock);
                                r = pthread_cond_timedwait(&d->cv_nonempty, &d->mtx, &ts);
                                if (r == ETIMEDOUT) {
                                        timedout = 1;
                                        break;
                                }
                        }
                        continue;
                }
                /// queue is non-empty
                if (d->defer_active) {
                        uint32_t wms = (wait_timeout_ms == 0) ? 0U : remaining_ms;

                        if (d->defer_until_ms != 0) {
                                uint64_t now = now_ms_clk(d->cv_clock);
                                if (now >= d->defer_until_ms) {
                                        d->defer_active = 0;
                                        d->defer_until_ms = 0;
                                        d->defer_seq = d->wake_seq;
                                        break; /* retry step */
                                }
                                uint64_t rem = d->defer_until_ms - now;
                                
                                /// Choose wait slice:
                                /// - caller timeout dominates when wait_timeout_ms != 0 (wms starts as remaining_ms)
                                /// - defer-until dominates when wait_timeout_ms == 0 (wms==0 => use rem)
                                /// Also clamp rem to uint32 range to avoid wrap-to-0.
                                if (wms == 0 || (uint64_t)wms > rem) {
                                        if (rem > (uint64_t)UINT32_MAX) {
                                                wms = UINT32_MAX;
                                        } else {
                                                wms = (uint32_t)rem;
                                        }
                                }
                        } else {
                                /// indefinite defer: if caller did not request timeout, wait indefinitely
                                if (wms == 0) {
                                        /// latched wake: if wake_seq advanced since defer activated, don't sleep
                                        if (d->wake_seq != d->defer_seq) {
                                                d->defer_active = 0;
                                                d->defer_until_ms = 0;
                                                d->defer_seq = d->wake_seq;
                                                break;  /// retry step
                                        }
                                        r = pthread_cond_wait(&d->cv_nonempty, &d->mtx);
                                        (void)r;
                                        if (d->wake_seq != d->defer_seq) {
                                                d->defer_active = 0;
                                                d->defer_until_ms = 0;
                                                d->defer_seq = d->wake_seq;
                                                break; /// retry step
                                        }
                                        continue; /// still deferred
                                }
                        }
                        /// timed wait path (either timed-defer, or indefinite-defer with caller timeout)
                        if (wms == 0) {
                                /// Defensive: avoid tight loop on unexpected 0ms slice.
                                /// (e.g., rem cast/rounding edge cases)
                                wms = 1U;
                                //continue;
                        }
                        struct timespec ts;
                        make_abs_timespec_ms(&ts, wms, d->cv_clock);
                        r = pthread_cond_timedwait(&d->cv_nonempty, &d->mtx, &ts);
                        if (r == ETIMEDOUT) {
                                /// total-timeout semantics: caller timeout dominates.
                                if (wait_timeout_ms != 0 && d->defer_until_ms == 0) {
                                        timedout = 1;
                                        break;
                                }
                                /// timed-defer slice ended: loop to recompute remaining_ms / check until_ms
                                continue;
                        }
                        /// woke up: if it was a latched wake, clear defer and retry step
                        if (d->defer_until_ms == 0 && d->wake_seq != d->defer_seq) {
                                d->defer_active = 0;
                                d->defer_until_ms = 0;
                                d->defer_seq = d->wake_seq;
                                break;
                        }
                        continue;
                }
                break; /// non-empty and not deferred
        }
        pthread_mutex_unlock(&d->mtx);
        if (timedout) {
                return 0U;
        }
        return 1U;
}

/// @brief Pop an event from the dispatcher and get the dispatch decision
/// @param d Pointer to the dispatcher structure
/// @param out Pointer to store the popped event and related info
/// @return dispatch_pop_rc_t code indicating the result of the pop operation
dispatch_pop_rc_t dispatch_pop(dispatch_t *d, dispatch_pop_result_t *out)
{
        dispatch_qnode_t node;
        uint32_t head_idx = 0;
        int got;
        uint64_t active_epoch = 0;
        int epoch_mismatch = 0;
        int req_canceled = 0;
        dispatch_decision_t dec;
        uint32_t defer_wait_ms = 0;
        int32_t defer_reason = 0;

        if (!d || !out) {
                return DISPATCH_POP_ERR;
        }

        memset(out, 0, sizeof(*out));

        pthread_mutex_lock(&d->mtx);
        got = peek_one_locked(d, &node, &head_idx);
        if (got) {
                active_epoch = d->active_epoch;
                epoch_mismatch = (node.epoch != active_epoch);
                req_canceled = is_req_canceled_locked(d, node.evt.req_id);
        }
        pthread_mutex_unlock(&d->mtx);

        if (!got) {
                out->rc = DISPATCH_POP_EMPTY;
                return out->rc;
        }

        if (epoch_mismatch) {
                emit_drop(d, DISPATCH_OBS_DROP_EPOCH, &node.evt);

                pthread_mutex_lock(&d->mtx);
                d->defer_active = 0;
                d->defer_until_ms = 0;
                (void)pop_commit_locked(d, head_idx);
                pthread_mutex_unlock(&d->mtx);

                out->evt = node.evt;
                out->epoch = node.epoch;
                out->rc = DISPATCH_POP_DROP_EPOCH;
                return out->rc;
        }

        if (req_canceled) {
                emit_drop(d, DISPATCH_OBS_DROP_REQ, &node.evt);

                pthread_mutex_lock(&d->mtx);
                d->defer_active = 0;
                d->defer_until_ms = 0;
                (void)pop_commit_locked(d, head_idx);
                pthread_mutex_unlock(&d->mtx);

                out->evt = node.evt;
                out->epoch = node.epoch;
                out->rc = DISPATCH_POP_DROP_REQ;
                return out->rc;
        }

        if (node.evt.ev == d->ev_cancel) {
                pthread_mutex_lock(&d->mtx);
                d->defer_active = 0;
                d->defer_until_ms = 0;
                (void)pop_commit_locked(d, head_idx);
                pthread_mutex_unlock(&d->mtx);

                emit_cancel_all(d, active_epoch);

                out->evt = node.evt;
                out->epoch = node.epoch;
                out->rc = DISPATCH_POP_CTRL_CANCEL_ALL;
                return out->rc;
        }

        if (node.evt.ev == d->ev_cancel_req) {
                pthread_mutex_lock(&d->mtx);
                d->defer_active = 0;
                d->defer_until_ms = 0;
                (void)pop_commit_locked(d, head_idx);
                pthread_mutex_unlock(&d->mtx);

                emit_cancel_req(d, (uint32_t)node.evt.a, active_epoch);

                out->evt = node.evt;
                out->epoch = node.epoch;
                out->rc = DISPATCH_POP_CTRL_CANCEL_REQ;
                return out->rc;
        }

        dec = d->policy.decide(d->policy.user, d, &node.evt);
        if (dec != DISPATCH_DECIDE_CONSUME &&
            dec != DISPATCH_DECIDE_DROP &&
            dec != DISPATCH_DECIDE_DEFER) {
                emit_drop(d, DISPATCH_OBS_DROP_POLICY, &node.evt);

                pthread_mutex_lock(&d->mtx);
                d->defer_active = 0;
                d->defer_until_ms = 0;
                (void)pop_commit_locked(d, head_idx);
                pthread_mutex_unlock(&d->mtx);

                out->evt = node.evt;
                out->epoch = node.epoch;
                out->rc = DISPATCH_POP_DROP_POLICY;
                return out->rc;
        }

        if (dec == DISPATCH_DECIDE_DEFER) {
                if (d->policy.defer_wait_ms) {
                        defer_wait_ms = d->policy.defer_wait_ms(d->policy.user, d, &node.evt);
                }
                if (d->policy.defer_reason) {
                        defer_reason = d->policy.defer_reason(d->policy.user, d, &node.evt);
                }

                pthread_mutex_lock(&d->mtx);
                d->defer_active = 1;
                d->defer_seq = d->wake_seq;
                if (defer_wait_ms == 0) {
                        d->defer_until_ms = 0;
                } else {
                        uint64_t now = now_ms_clk(d->cv_clock);
                        d->defer_until_ms = now + (uint64_t)defer_wait_ms;
                }
                pthread_mutex_unlock(&d->mtx);

                emit_defer(d, (int32_t)node.evt.ev, node.evt.req_id, defer_wait_ms, defer_reason);

                out->evt = node.evt;
                out->epoch = node.epoch;
                out->defer_wait_ms = defer_wait_ms;
                out->defer_reason = defer_reason;
                out->rc = DISPATCH_POP_DEFER;
                return out->rc;
        }

        if (dec == DISPATCH_DECIDE_DROP) {
                emit_drop(d, DISPATCH_OBS_DROP_POLICY, &node.evt);

                pthread_mutex_lock(&d->mtx);
                d->defer_active = 0;
                d->defer_until_ms = 0;
                (void)pop_commit_locked(d, head_idx);
                pthread_mutex_unlock(&d->mtx);

                out->evt = node.evt;
                out->epoch = node.epoch;
                out->rc = DISPATCH_POP_DROP_POLICY;
                return out->rc;
        }

        pthread_mutex_lock(&d->mtx);
        d->defer_active = 0;
        d->defer_until_ms = 0;
        (void)pop_commit_locked(d, head_idx);
        pthread_mutex_unlock(&d->mtx);

        out->evt = node.evt;
        out->epoch = node.epoch;
        out->rc = DISPATCH_POP_OK;

        emit_dispatch(d, &node.evt, (int32_t)DISPATCH_POP_OK);
        return out->rc;
}

/// @brief Wake up consumer threads (e.g., after canceling requests or changing state)
/// @param d Pointer to the dispatcher structure
void dispatch_wakeup(dispatch_t *d)
{
        if (!d) {
                return;
        }
        pthread_mutex_lock(&d->mtx);
        d->wake_seq++;
        pthread_cond_broadcast(&d->cv_nonempty);
        pthread_mutex_unlock(&d->mtx);
}

/// @brief Get the number of used slots in the dispatcher's queue
/// @param d Pointer to the dispatcher structure
/// @return Number of used slots
uint32_t dispatch_q_used(dispatch_t *d)
{
        uint32_t u = 0;
        if (!d) {
                return 0;
        }

        pthread_mutex_lock(&d->mtx);
        u = q_used_n(d);
        pthread_mutex_unlock(&d->mtx);
        return u;
}

/// @brief Get the capacity of the dispatcher's queue
/// @param d Pointer to the dispatcher structure
/// @return Capacity of the queue
uint32_t dispatch_q_cap(dispatch_t *d)
{
        return d ? d->cap : 0;
}

/// @brief Get the number of used slots in the dispatcher's queue (thread-safe)
/// @param d Pointer to the dispatcher structure
/// @return Number of used slots
uint32_t dispatch_get_used(const dispatch_t *d)
{
        return dispatch_q_used((dispatch_t *)d);
}

/// @brief Get the capacity of the dispatcher's queue (thread-safe)
/// @param d Pointer to the dispatcher structure
/// @return Capacity of the queue
uint32_t dispatch_get_cap(const dispatch_t *d)
{
        return dispatch_q_cap((dispatch_t *)d);
}

/// @brief Get the current active epoch of the dispatcher
/// @param d Pointer to the dispatcher structure
/// @return Current active epoch
uint64_t dispatch_epoch_get(dispatch_t *d)
{
        uint64_t e = 0;
        if (!d) {
                return 0;
        }

        pthread_mutex_lock(&d->mtx);
        e = d->active_epoch;
        pthread_mutex_unlock(&d->mtx);
        return e;
}

/// @brief Cancel a specific request in the dispatcher.
/// @param d Pointer to the dispatcher structure pointer.
/// @param req_id Request ID to cancel
void dispatch_cancel_req(dispatch_t *d, uint32_t req_id)
{
        uint64_t epoch;

        if (!d || req_id == 0) {
                return;
        }

        pthread_mutex_lock(&d->mtx);

        if (!d->cancel_tab || d->cancel_tab_cap == 0) {
                pthread_mutex_unlock(&d->mtx);
                return;
        }
        epoch = d->active_epoch;

        (void)cancel_tab_insert_locked(d, req_id, epoch);
        pthread_mutex_unlock(&d->mtx);

        dispatch_evt_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.ev = d->ev_cancel_req;
        ev.req_id = 0; // control-plane: not using req_id.
        ev.a = (int32_t)req_id; // cancle req_id payload in 'a'
        (void)dispatch_push(d, &ev);
}

/// @brief Cancel all requests in the dispatcher
/// @param d Pointer to the dispatcher structure
/// @param flush_q 1: flush the queue, 0: keep the queue
/// @param push_cancel_event 1: push a cancel event, 0: do not push
void dispatch_cancel_all(dispatch_t *d, bool flush_q, uint32_t push_cancel_event)
{
        if (!d) {
                return;
        }

        pthread_mutex_lock(&d->mtx);

        d->active_epoch++; 

        if (flush_q) {
                d->head = d->tail;
                d->defer_active = 0;
                d->defer_until_ms = 0;
                pthread_cond_broadcast(&d->cv_nonfull);
        }

        pthread_mutex_unlock(&d->mtx);

        if (push_cancel_event) {
                dispatch_evt_t e;
                memset(&e, 0, sizeof(e));
                e.ev = d->ev_cancel;
                e.req_id = 0;
                (void)dispatch_push(d, &e);
        }
}