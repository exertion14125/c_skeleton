/// @file       log_util.h
/// @brief      Provides simple and detailed logging macros for various log levels.
/// @author     KIM JEONGGI (jgkim12@digitech.kr)

#ifndef __LOG_UTIL_H__
#define __LOG_UTIL_H__

#include "log_level.h"
#include "log_system.h"

#ifndef LOG_COMPILE_LEVEL
#ifdef DEBUG
#define LOG_COMPILE_LEVEL LOG_LVL_DBG
#else
#define LOG_COMPILE_LEVEL LOG_LVL_INF
#endif
#endif

#ifndef APP_EXEC_NAME
#define C_APP_EXEC_NAME "APP"
#else
#define C_APP_EXEC_NAME APP_EXEC_NAME
#endif

/// Log macros without file, line, and function info
#define LOGX_TAG(lvl, tag, fmt, ...) do {                                 \
        if ((lvl) >= (LOG_COMPILE_LEVEL)) {                               \
                write_log((lvl), (tag), "", 0, "", (fmt), ##__VA_ARGS__); \
        }                                                                 \
} while (0)

#define LOGD(fmt, ...) LOGX_TAG(LOG_LVL_DBG, C_APP_EXEC_NAME, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) LOGX_TAG(LOG_LVL_INF, C_APP_EXEC_NAME, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) LOGX_TAG(LOG_LVL_WAR, C_APP_EXEC_NAME, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) LOGX_TAG(LOG_LVL_ERR, C_APP_EXEC_NAME, fmt, ##__VA_ARGS__)

#define LOGD_T(tag, fmt, ...) LOGX_TAG(LOG_LVL_DBG, (tag), fmt, ##__VA_ARGS__)
#define LOGI_T(tag, fmt, ...) LOGX_TAG(LOG_LVL_INF, (tag), fmt, ##__VA_ARGS__)
#define LOGW_T(tag, fmt, ...) LOGX_TAG(LOG_LVL_WAR, (tag), fmt, ##__VA_ARGS__)
#define LOGE_T(tag, fmt, ...) LOGX_TAG(LOG_LVL_ERR, (tag), fmt, ##__VA_ARGS__)

/// Detailed log macros with file, line, and function info
#define FLOGX_TAG(lvl, tag, fmt, ...) do {                                                      \
        if ((lvl) >= (LOG_COMPILE_LEVEL)) {                                                     \
                write_log((lvl), (tag), __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__);    \
        }                                                                                       \
} while (0)

#define FLOGD(fmt, ...) FLOGX_TAG(LOG_LVL_DBG, C_APP_EXEC_NAME, fmt, ##__VA_ARGS__)
#define FLOGI(fmt, ...) FLOGX_TAG(LOG_LVL_INF, C_APP_EXEC_NAME, fmt, ##__VA_ARGS__)
#define FLOGW(fmt, ...) FLOGX_TAG(LOG_LVL_WAR, C_APP_EXEC_NAME, fmt, ##__VA_ARGS__)
#define FLOGE(fmt, ...) FLOGX_TAG(LOG_LVL_ERR, C_APP_EXEC_NAME, fmt, ##__VA_ARGS__)

#define FLOGD_T(tag, fmt, ...) FLOGX_TAG(LOG_LVL_DBG, (tag), fmt, ##__VA_ARGS__)
#define FLOGI_T(tag, fmt, ...) FLOGX_TAG(LOG_LVL_INF, (tag), fmt, ##__VA_ARGS__)
#define FLOGW_T(tag, fmt, ...) FLOGX_TAG(LOG_LVL_WAR, (tag), fmt, ##__VA_ARGS__)
#define FLOGE_T(tag, fmt, ...) FLOGX_TAG(LOG_LVL_ERR, (tag), fmt, ##__VA_ARGS__)

#endif /* __LOG_UTIL_H__ */