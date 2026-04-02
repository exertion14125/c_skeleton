#ifndef __UTIL_OBS_TYPES_H__
#define __UTIL_OBS_TYPES_H__

#include <stdint.h>

/// @brief Observation event types
typedef enum {
        OBS_EVT_FSM = 1,
        OBS_EVT_DISPATCH = 2,
        
        OBS_EVT_DROP = 10,
        OBS_EVT_DROP_EPOCH = 11,
        OBS_EVT_DROP_REQ = 12,
        OBS_EVT_DROP_POLICY = 13,
        OBS_EVT_DROP_FULL = 14,
        
        OBS_EVT_CANCEL_ALL = 20,
        OBS_EVT_CANCEL_REQ = 21,
        
        OBS_EVT_DEFER = 30,

        OBS_EVT_FSM_TR = 200,   
        OBS_EVT_FSM_ENTER,      
        OBS_EVT_FSM_EXIT,       
        OBS_EVT_FSM_GUARD_FAIL, 
        OBS_EVT_FSM_NOOP,
        OBS_EVT_FSM_STEP,
} obs_evt_type_t;

/// @brief Observation event record
/// @note DROP_: obs_push() a=ev, b=req_id, c= reason code (if any)
/// @note CANCEL_: obs_push() a=req_id, b=epoch_low32, c=epoch_high32
struct obs_evt_s {
        uint64_t ts_ms; ///< timestamp in milliseconds
        uint32_t type; ///< event type (obs_evt_type_t)
        int32_t  a; ///< event-specific data
        int32_t  b; ///< event-specific data
        int32_t  c; ///< event-specific data
};
typedef struct obs_evt_s obs_evt_t;

/// @brief Observation snapshot record
struct obs_snapshot_s {
        uint64_t epoch; ///< epoch number
        uint64_t push_cnt; ///< number of pushed events
        uint64_t drop_cnt; ///< number of dropped events
        uint32_t used; ///< number of used slots
        uint32_t cap; ///< capacity of the snapshot
};
typedef struct obs_snapshot_s obs_snapshot_t;

#define OBS_DEFER_PACK(reason, wait_ms) ( (int32_t)((((uint32_t)(reason) & 0xFFFFU) << 16) | ((uint32_t)(wait_ms) & 0xFFFFU)) )
#define OBS_DEFER_WAIT_MS(packed_c) ( (uint32_t)((uint32_t)(packed_c) & 0xFFFFU) )
#define OBS_DEFER_REASON(packed_c)  ( (int32_t)(((uint32_t)(packed_c) >> 16) & 0xFFFFU) )

/// @brief Observation KPI counters (producer-side counting)
/// @note These counters are updated even when the ring is full (best-effort visibility).
struct obs_kpi_s {
        uint64_t fsm_cnt;
        uint64_t dispatch_cnt;
        uint64_t cancel_all_cnt;
        uint64_t cancel_req_cnt;

        uint64_t drop_epoch_cnt;
        uint64_t drop_req_cnt;
        uint64_t drop_policy_cnt;
        uint64_t drop_full_cnt;

        uint64_t defer_cnt;
};
typedef struct obs_kpi_s obs_kpi_t;
#endif /* __UTIL_OBS_TYPES_H__ */