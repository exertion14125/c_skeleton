/// @file       log_types.h
/// @brief      Defines common types and structures used across the logging system.
/// @author     KIM JEONGGI (jgkim12@digitech.kr)

#ifndef __LOG_TYPES_H__
#define __LOG_TYPES_H__

#include <stdint.h>
#include <stddef.h>

#include "util/log/log_level.h"

struct log_record_s {
        uint64_t ts_ms; ///< Timestamp in milliseconds.
        log_level_t level; ///< Log message level.
        const char *tag; ///< Log tag.
        const char *file; ///< Source file name.
        int line; ///< Source line number.
        const char *func;///< Source function name.
        const char *msg; ///< Log message.
};
typedef struct log_record_s log_record_t;
#endif /* __LOG_TYPES_H__ */