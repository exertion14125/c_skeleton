#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "util/obs/obs.h"

/// @brief Observation structure
struct obs_s {
        obs_evt_t *ring;
        uint32_t cap;
        uint32_t head; /* pop */
        uint32_t tail; /* push */
        uint64_t epoch;
        uint64_t push_cnt;
        uint64_t drop_cnt;
        obs_kpi_t kpi;
        obs_now_ms_fn now_fn;
        void *now_user;
};

/// @brief Set KPI counters
/// @param o Pointer to the observation structure
/// @param type Event type
static void kpi_count(obs_t *o, uint32_t type)
{
        if (!o) {
                return;
        }
        switch (type) {
        case OBS_EVT_FSM:         o->kpi.fsm_cnt++; break;
        case OBS_EVT_DISPATCH:    o->kpi.dispatch_cnt++; break;
        case OBS_EVT_CANCEL_ALL:  o->kpi.cancel_all_cnt++; break;
        case OBS_EVT_CANCEL_REQ:  o->kpi.cancel_req_cnt++; break;
        case OBS_EVT_DROP_EPOCH:  o->kpi.drop_epoch_cnt++; break;
        case OBS_EVT_DROP_REQ:    o->kpi.drop_req_cnt++; break;
        case OBS_EVT_DROP_POLICY: o->kpi.drop_policy_cnt++; break;
        case OBS_EVT_DROP_FULL:   o->kpi.drop_full_cnt++; break;
        case OBS_EVT_DEFER:       o->kpi.defer_cnt++; break;
        default:
                break;
        }
}

/// @brief Get number of used slots in the ring buffer
static uint32_t used_n(const obs_t *o)
{
        if (o->tail >= o->head) {
                return (o->tail - o->head);
        }
        return (o->cap - (o->head - o->tail));
}

/// @brief Allocate the observation structure
/// @return Pointer to the allocated observation structure, or NULL on failure 
obs_t* alloc_obs(void)
{
        return (obs_t*)calloc(1, sizeof(obs_t));
}

/// @brief Destroy the observation structure
/// @param o Pointer to the observation structure pointer
void destroy_obs(obs_t **po)
{
        if (po && *po) {
                free(*po);
                *po = NULL;
        }
}


/// @brief Initialize the observation structure
/// @param o Pointer to the observation structure
/// @param ring_mem Memory for the ring buffer
/// @param ring_cap Capacity of the ring buffer
/// @param now_fn Function to get the current time in milliseconds
/// @param now_user User data for the now_fn function
/// @return 0 on success, -1 on failure
int obs_init(obs_t *o, obs_evt_t *ring_mem, uint32_t ring_cap, obs_now_ms_fn now_fn, void *now_user)
{
        if (!o || !ring_mem || ring_cap < 2 || !now_fn) {
                return -1;
        }
        memset(o, 0, sizeof(*o));
        o->ring = ring_mem;
        o->cap = ring_cap;
        o->now_fn = now_fn;
        o->now_user = now_user;
        return 0;
}

/// @brief Increment the epoch number
/// @param o Pointer to the observation structure
void obs_epoch_inc(obs_t *o)
{
        if (o) {
                o->epoch++;
        }
}

/// @brief Get the current epoch number
/// @param o Pointer to the observation structure
/// @return Current epoch number
uint64_t obs_epoch_get(const obs_t *o)
{
        return o ? o->epoch : 0;
}

/// @brief Push an event into the observation structure
/// @param o Pointer to the observation structure
/// @param type Event type
/// @param a Event data a
/// @param b Event data b
/// @param c Event data c
/// @return 1 if pushed, 0 if dropped (full), <0 on error
int obs_push(obs_t *o, uint32_t type, int32_t a, int32_t b, int32_t c)
{
        uint32_t next;
        obs_evt_t *e;

        if (!o) {
                return -1;
        }
        /// KPI is counted even when ring is full (best-effort visibility)
        kpi_count(o, type);

        next = o->tail + 1;
        if (next >= o->cap) {
                next = 0;
        }

        if (next == o->head) {
                o->drop_cnt++;
                return 0;
        }

        e = &o->ring[o->tail];
        e->ts_ms = o->now_fn(o->now_user);
        e->type = type;
        e->a = a; e->b = b; e->c = c;

        o->tail = next;
        o->push_cnt++;
        return 1;
}

/// @brief Pop an event from the observation structure
/// @param o Pointer to the observation structure
/// @param out Pointer to store the popped event
/// @return 1 if got, 0 if empty, <0 on error
int obs_pop(obs_t *o, obs_evt_t *out)
{
        if (!o || !out) {
                return -1;
        }
        if (o->head == o->tail) {
                return 0;
        }
        *out = o->ring[o->head];
        o->head++;
        if (o->head >= o->cap) {
                o->head = 0;
        }
        return 1;
}

/// @brief Get a snapshot of the observation structure
/// @param o Pointer to the observation structure
/// @param out Pointer to store the snapshot
void obs_get_snapshot(const obs_t *o, obs_snapshot_t *out)
{
        if (!o || !out) {
                return;
        }
        out->epoch = o->epoch;
        out->push_cnt = o->push_cnt;
        out->drop_cnt = o->drop_cnt;
        out->used = used_n(o);
        out->cap = o->cap;
}

/// @brief Get KPI counters
/// @param o Pointer to the observation structure
/// @param out Pointer to store the KPI counters
void obs_get_kpi(const obs_t *o, obs_kpi_t *out)
{
        if (!o || !out) {
                return;
        }
        *out = o->kpi;
}