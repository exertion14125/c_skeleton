#ifndef __UTIL_FSM_DECL_H__
#define __UTIL_FSM_DECL_H__

#include <stdint.h>

#define FSM_TABLE_LEN(a) ((uint32_t)(sizeof(a) / sizeof((a)[0]))) ///< get number of entries in fsm table

#endif /* __UTIL_FSM_DECL_H__ */