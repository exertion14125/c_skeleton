#ifndef __UTIL_FSM_OBSERVER_IF_H__
#define __UTIL_FSM_OBSERVER_IF_H__

#include <stdint.h>
#include "util/fsm/fsm_types.h"

struct fsm_observer_vtbl_s {
        /*
         * before     : step 시작 상태
         * next_hint  : 매칭된 transition의 next (NOOP이면 before, GUARD_BLOCK이면 후보 next)
         * after      : step 종료 상태 (OK면 next, 그 외 before)
         * rc         : fsm_step_rc_t
         */
        void (*on_step)(void *user, fsm_state_t before, fsm_event_t ev, fsm_state_t next_hint, fsm_state_t after, fsm_step_rc_t rc);

        /* 실제 전이(OK && before!=after)일 때만 */
        void (*on_transition)(void *user, fsm_state_t before, fsm_event_t ev, fsm_state_t next_hint, fsm_state_t after);

        /* 실제 전이(OK && before!=after)일 때만 */
        void (*on_exit)(void *user, fsm_state_t before, fsm_event_t ev, fsm_state_t after);
        void (*on_enter)(void *user, fsm_state_t after, fsm_event_t ev, fsm_state_t before);

        /* 가드 실패(GUARD_BLOCK)일 때만 */
        void (*on_guard_fail)(void *user, fsm_state_t st, fsm_event_t ev, fsm_state_t next_hint);

        /* 전이 없음(NOOP)일 때만 */
        void (*on_noop)(void *user, fsm_state_t before, fsm_event_t ev, fsm_state_t next_hint);
};

typedef struct fsm_observer_vtbl_s fsm_observer_vtbl_t;

struct fsm_observer_s {
        const fsm_observer_vtbl_t *vtbl;
        void *user;
};
typedef struct fsm_observer_s fsm_observer_t;

#endif /* __UTIL_FSM_OBSERVER_IF_H__ */
