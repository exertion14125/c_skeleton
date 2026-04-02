#ifndef __UTIL_FSM_TYPES_H__
#define __UTIL_FSM_TYPES_H__

#include <stdint.h>

typedef int32_t fsm_state_t;
typedef int32_t fsm_event_t;

/// @brief Return codes for fsm_step function
typedef enum {
        FSM_STEP_OK = 0,           ///< transition happened
        FSM_STEP_NOOP = 1,         ///< no transition
        FSM_STEP_GUARD_BLOCK = 2,  ///< guard rejected
        FSM_STEP_ERR = -1          ///< invalid args
} fsm_step_rc_t;

#endif /* __UTIL_FSM_TYPES_H__ */