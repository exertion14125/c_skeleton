#ifndef __RESOURCE_RED_RED_STATE_DTO_H__
#define __RESOURCE_RED_RED_STATE_DTO_H__

#include <stdint.h>

typedef struct red_state_dto_s {
        uint32_t role;
        uint32_t peer_state;
        uint32_t health;
        uint32_t heartbeat_seq;
        uint64_t ts_ms;
        uint32_t flags;
} red_state_dto_t;

#endif /* __RESOURCE_RED_RED_STATE_DTO_H__ */