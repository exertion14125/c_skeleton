/// @file       log_level.h
/// @brief      Provides log level definitions and utility functions.
/// @author     KIM JEONGGI (jgkim12@digitech.kr)

#ifndef __LOG_LEVEL_H__
#define __LOG_LEVEL_H__

/// @brief Log message level.
typedef enum {
        LOG_LVL_DBG = 0,
        LOG_LVL_INF = 1,
        LOG_LVL_WAR = 2,
        LOG_LVL_ERR = 3,
} log_level_t;

/// @brief Log level string.
/// @param level Log level.
/// @return Log level string.
static inline const char* log_level_str(log_level_t level) 
{
        switch (level) {
                case LOG_LVL_DBG: return "DBG";
                case LOG_LVL_INF: return "INF";
                case LOG_LVL_WAR: return "WRN";
                case LOG_LVL_ERR: return "ERR";
                default:          return "UNK";
        }
}

#endif /* __LOG_LEVEL_H__ */