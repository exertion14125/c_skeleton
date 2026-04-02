/// @file       def_debug.h
/// @brief      Provides macros and functions for debug with various features.
/// @author     KIM JEONGGI (jgkim12@digitech.kr)

#ifndef _DEF_DEBUG_H_
#define _DEF_DEBUG_H_

// #define DEBUG
#ifdef DEBUG
// #define THREAD_SAFE       /// Uncomment to enable thread-safe logging
#define FLUSH_OUTPUT         /// Uncomment to enable immediate output flushing
#define COLOR_OUTPUT         /// Uncomment to enable colored output
#define POSITION_OUTPUT      /// Uncomment to enable position-controlled output
#endif

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#ifdef THREAD_SAFE
#include <pthread.h>
#endif

/// Log level definitions for LDMSG, CDMSG, and PMSG macros
typedef enum {
        DBG_INF,
        DBG_WAR,
        DBG_ERR
} dbg_level_t;

#ifdef COLOR_OUTPUT
/// Color codes for log levels
#define COLOR_INF       "\033[32m" ///       Green
#define COLOR_WAR       "\033[33m" ///       Yellow
#define COLOR_ERR       "\033[31m" ///       Red
#define COLOR_RST       "\033[0m"
#define GET_COLOR(level) \
        ((level) == DBG_INF ? COLOR_INF : \
         (level) == DBG_WAR ? COLOR_WAR : COLOR_ERR)
#else
#define GET_COLOR(level) ""
#define COLOR_RST        ""
#endif

// Define these once based on POSITION_OUTPUT
#ifdef POSITION_OUTPUT
/// Set cursor position: row, col (1-based)
#define SET_POSITION_RAW(row, col) \
        do { if ((row) > 0 && (col) > 0) fprintf(DEBUG_MESSAGE_FILE, "\033[%d;%dH", (row), (col)); } while(0)
/// Clear entire screen
#define CLR_SCREEN_RAW() \
        do { fprintf(DEBUG_MESSAGE_FILE, "\033[2J"); } while(0)
/// Clear line from cursor to end
#define CLR_LINE_RAW() \
        do { fprintf(DEBUG_MESSAGE_FILE, "\033[K"); } while(0)
#else
#define SET_POSITION_RAW(row, col) do {} while(0)
#define CLR_SCREEN_RAW() do {} while(0)
#define CLR_LINE_RAW() do {} while(0)
#endif


#ifdef DEBUG
#ifdef THREAD_SAFE
/// Global mutex for thread-safe logging
static pthread_mutex_t debug_mutex = PTHREAD_MUTEX_INITIALIZER;
#define DEBUG_LOCK()   pthread_mutex_lock(&debug_mutex)
#define DEBUG_UNLOCK() pthread_mutex_unlock(&debug_mutex)
#else
#define DEBUG_LOCK()   do {} while(0)
#define DEBUG_UNLOCK() do {} while(0)
#endif

#ifdef FLUSH_OUTPUT
#define DEBUG_FLUSH()  fflush(DEBUG_MESSAGE_FILE)
#else
#define DEBUG_FLUSH()  do {} while(0)
#endif

#define DEBUG_MESSAGE_FILE      stdout

///       Convert log level to string for LDMSG, CDMSG, and PMSG macros
#define DEBUG_LEVEL_STR(level) \
        ((level) == DBG_INF ? "INF" : \
         (level) == DBG_WAR ? "WAR" : "ERR")

///       Basic macros (no log level)
#define DMSG(msgs...) do {\
        char ts[82];\
        dbg_get_timestamp(ts, sizeof(ts));\
        DEBUG_LOCK();\
        fprintf(DEBUG_MESSAGE_FILE, "[%s] ", ts);\
        fprintf(DEBUG_MESSAGE_FILE, msgs);\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

#define DMSG_FULL(msgs...) do {\
        char ts[82];\
        dbg_get_timestamp(ts, sizeof(ts));\
        DEBUG_LOCK();\
        fprintf(DEBUG_MESSAGE_FILE, "[%s]%s:%4d: %s(): ", \
                ts, __FILE__, __LINE__, __FUNCTION__);\
        fprintf(DEBUG_MESSAGE_FILE, msgs);\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

#define DMSG_SHORT(msgs...) do {\
        char ts[82];\
        dbg_get_timestamp(ts, sizeof(ts));\
        DEBUG_LOCK();\
        fprintf(DEBUG_MESSAGE_FILE, "[%s]%s(): ", ts, __FUNCTION__);\
        fprintf(DEBUG_MESSAGE_FILE, msgs);\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

#define DMSG_ERR(msgs...) do {\
        char ts[82];\
        dbg_get_timestamp(ts, sizeof(ts));\
        DEBUG_LOCK();\
        fprintf(DEBUG_MESSAGE_FILE, "[%s][ERROR]%s(): ", ts, __FUNCTION__);\
        fprintf(DEBUG_MESSAGE_FILE, msgs);\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

#define DMSG_BYTE2HEX(data, len) do {                                                                           \
        const unsigned char *__dmsg_p = (const unsigned char *)(data);                                          \
        size_t __dmsg_len = (size_t)(len);                                                                      \
        size_t __dmsg_i, __dmsg_j;                                                                              \
        /* print timestamp once */                                                                              \
        char __dmsg_ts[32];                                                                                     \
        dbg_get_timestamp(__dmsg_ts, sizeof(__dmsg_ts));                                                        \
        DEBUG_LOCK();                                                                                           \
        fprintf(DEBUG_MESSAGE_FILE, "[%s] HEXDUMP (%zu bytes)\n", __dmsg_ts, __dmsg_len);                       \
        DEBUG_FLUSH();                                                                                          \
        DEBUG_UNLOCK();                                                                                         \
        /* now print data without timestamp */                                                                  \
        for (__dmsg_i = 0; __dmsg_i < __dmsg_len; __dmsg_i++) {                                                 \
                if ((__dmsg_i & 0x0F) == 0) {                                                                   \
                        fprintf(DEBUG_MESSAGE_FILE, "%04x  ", (unsigned int)__dmsg_i);                          \
                }                                                                                               \
                fprintf(DEBUG_MESSAGE_FILE, "%02x ", __dmsg_p[__dmsg_i]);                                       \
                if ((__dmsg_i & 0x0F) == 0x0F) {                                                                \
                        fprintf(DEBUG_MESSAGE_FILE, " ");                                                       \
                        for (__dmsg_j = __dmsg_i - 15; __dmsg_j <= __dmsg_i; __dmsg_j++) {                      \
                                unsigned char __dmsg_c = __dmsg_p[__dmsg_j];                                    \
                                fprintf(DEBUG_MESSAGE_FILE, "%c", (__dmsg_c >= 32 && __dmsg_c <= 126) ? __dmsg_c : '.');\
                        }                                                                                       \
                }                                                                                               \
        }                                                                                                       \
        do {                                                                                                    \
                size_t __dmsg_rem = (__dmsg_len & 0x0F);                                                        \
                if (__dmsg_rem != 0) {                                                                          \
                        for (__dmsg_i = __dmsg_rem; __dmsg_i < 16; __dmsg_i++) {                                \
                                fprintf(DEBUG_MESSAGE_FILE, "   ");                                             \
                        }                                                                                       \
                        fprintf(DEBUG_MESSAGE_FILE, " ");                                                       \
                        for (__dmsg_i = __dmsg_len - __dmsg_rem; __dmsg_i < __dmsg_len; __dmsg_i++) {           \
                                unsigned char __dmsg_c2 = __dmsg_p[__dmsg_i];                                   \
                                fprintf(DEBUG_MESSAGE_FILE, "%c", (__dmsg_c2 >= 32 && __dmsg_c2 <= 126) ? __dmsg_c2 : '.');\
                        }                                                                                       \
                }                                                                                               \
        } while (0);                                                                                            \
        fprintf(DEBUG_MESSAGE_FILE, "\n");                                                                                             \
} while (0)

// Log level macros
#define LDMSG(level, msgs...) do {\
        char ts[82];\
        dbg_get_timestamp(ts, sizeof(ts));\
        DEBUG_LOCK();\
        fprintf(DEBUG_MESSAGE_FILE, "[%s][%s] ", ts, DEBUG_LEVEL_STR(level));\
        fprintf(DEBUG_MESSAGE_FILE, msgs);\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

#define LDMSG_FULL(level, msgs...) do {\
        char ts[82];\
        dbg_get_timestamp(ts, sizeof(ts));\
        DEBUG_LOCK();\
        fprintf(DEBUG_MESSAGE_FILE, "[%s][%s]%s:%4d: %s(): ", \
                ts, DEBUG_LEVEL_STR(level), __FILE__, __LINE__, __FUNCTION__);\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

#define LDMSG_SHORT(level, msgs...) do {\
        char ts[82];\
        dbg_get_timestamp(ts, sizeof(ts));\
        DEBUG_LOCK();\
        fprintf(DEBUG_MESSAGE_FILE, "[%s][%s]%s(): ", \
                ts, DEBUG_LEVEL_STR(level), __FUNCTION__);\
        fprintf(DEBUG_MESSAGE_FILE, msgs);\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

#define LDMSG_ERR(msgs...) do {\
        char ts[82];\
        dbg_get_timestamp(ts, sizeof(ts));\
        DEBUG_LOCK();\
        fprintf(DEBUG_MESSAGE_FILE, "[%s][ERROR]%s(): ", ts, __FUNCTION__);\
        fprintf(DEBUG_MESSAGE_FILE, msgs);\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

// Color output macros
#define CDMSG(level, msgs...) do {\
        char ts[82];\
        dbg_get_timestamp(ts, sizeof(ts));\
        DEBUG_LOCK();\
        fprintf(DEBUG_MESSAGE_FILE, "%s[%s][%s] ", \
                GET_COLOR(level), ts, DEBUG_LEVEL_STR(level));\
        fprintf(DEBUG_MESSAGE_FILE, msgs);\
        fprintf(DEBUG_MESSAGE_FILE, "%s", COLOR_RST);\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

#define CDMSG_FULL(level, msgs...) do {\
        char ts[82];\
        dbg_get_timestamp(ts, sizeof(ts));\
        DEBUG_LOCK();\
        fprintf(DEBUG_MESSAGE_FILE, "%s[%s][%s]%s:%4d: %s(): ", \
                GET_COLOR(level), ts, DEBUG_LEVEL_STR(level), __FILE__, __LINE__, __FUNCTION__);\
        fprintf(DEBUG_MESSAGE_FILE, msgs);\
        fprintf(DEBUG_MESSAGE_FILE, "%s", COLOR_RST);\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

#define CDMSG_SHORT(level, msgs...) do {\
        char ts[82];\
        dbg_get_timestamp(ts, sizeof(ts));\
        DEBUG_LOCK();\
        fprintf(DEBUG_MESSAGE_FILE, "%s[%s][%s]%s(): ", \
                GET_COLOR(level), ts, DEBUG_LEVEL_STR(level), __FUNCTION__);\
        fprintf(DEBUG_MESSAGE_FILE, msgs);\
        fprintf(DEBUG_MESSAGE_FILE, "%s", COLOR_RST);\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

#define CDMSG_ERR(msgs...) do {\
        char ts[82];\
        dbg_get_timestamp(ts, sizeof(ts));\
        DEBUG_LOCK();\
        fprintf(DEBUG_MESSAGE_FILE, "%s[%s][ERROR]%s(): ", \
                GET_COLOR(DBG_LEVEL_ERROR), ts, __FUNCTION__);\
        fprintf(DEBUG_MESSAGE_FILE, msgs);\
        fprintf(DEBUG_MESSAGE_FILE, "%s", COLOR_RST);\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

// Position output macros
// Use SET_POSITION_RAW, CLEAR_SCREEN_RAW, CLEAR_LINE_RAW which are defined outside DEBUG block
// and handle POSITION_OUTPUT condition.
#define PMSG(level, row, col, msgs...) do {\
        char ts[82];\
        dbg_get_timestamp(ts, sizeof(ts)); /* Changed get_timestamp to dbg_get_timestamp */ \
        DEBUG_LOCK();\
        SET_POSITION_RAW(row, col);\
        fprintf(DEBUG_MESSAGE_FILE, "[%s][%s] ", ts, DEBUG_LEVEL_STR(level));\
        fprintf(DEBUG_MESSAGE_FILE, msgs);\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

#define PMSG_FULL(level, row, col, msgs...) do {\
        char ts[82];\
        dbg_get_timestamp(ts, sizeof(ts));\
        DEBUG_LOCK();\
        SET_POSITION_RAW(row, col);\
        fprintf(DEBUG_MESSAGE_FILE, "[%s][%s]%s:%4d: %s(): ", \
                ts, DEBUG_LEVEL_STR(level), __FILE__, __LINE__, __FUNCTION__);\
        fprintf(DEBUG_MESSAGE_FILE, msgs);\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

#define PMSG_SHORT(level, row, col, msgs...) do {\
        char ts[82];\
        dbg_get_timestamp(ts, sizeof(ts));\
        DEBUG_LOCK();\
        SET_POSITION_RAW(row, col);\
        fprintf(DEBUG_MESSAGE_FILE, "[%s][%s]%s(): ", \
                ts, DEBUG_LEVEL_STR(level), __FUNCTION__);\
        fprintf(DEBUG_MESSAGE_FILE, msgs);\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

#define PMSG_ERR(row, col, msgs...) do {\
        char ts[82];\
        dbg_get_timestamp(ts, sizeof(ts));\
        DEBUG_LOCK();\
        SET_POSITION_RAW(row, col);\
        fprintf(DEBUG_MESSAGE_FILE, "[%s][ERROR]%s(): ", ts, __FUNCTION__);\
        fprintf(DEBUG_MESSAGE_FILE, msgs);\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

///       Screen and line clear macros (now using RAW versions)
#define CLR_SCREEN() do {\
        DEBUG_LOCK();\
        CLR_SCREEN_RAW();\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

#define CLR_LINE() do {\
        DEBUG_LOCK();\
        CLR_LINE_RAW();\
        DEBUG_FLUSH();\
        DEBUG_UNLOCK();\
} while(0)

#else // #ifndef DEBUG (when DEBUG is NOT defined)
#define DMSG(msgs...) do {} while(0)
#define DMSG_FULL(msgs...) do {} while(0)
#define DMSG_SHORT(msgs...) do {} while(0)
#define DMSG_ERR(msgs...) do {} while(0)
#define DMSG_BYTE2HEX(data, len) do {} while(0)
#define LDMSG(level, msgs...) do {} while(0)
#define LDMSG_FULL(level, msgs...) do {} while(0)
#define LDMSG_SHORT(level, msgs...) do {} while(0)
#define LDMSG_ERR(msgs...) do {} while(0)
#define CDMSG(level, msgs...) do {} while(0)
#define CDMSG_FULL(level, msgs...) do {} while(0)
#define CDMSG_SHORT(level, msgs...) do {} while(0)
#define CDMSG_ERR(msgs...) do {} while(0)
#define PMSG(level, row, col, msgs...) do {} while(0)
#define PMSG_FULL(level, row, col, msgs...) do {} while(0)
#define PMSG_SHORT(level, row, col, msgs...) do {} while(0)
#define PMSG_ERR(row, col, msgs...) do {} while(0)
// Also ensure CLEAR_SCREEN and CLEAR_LINE are defined as no-ops if DEBUG is off
#define CLR_SCREEN() do {} while(0)
#define CLR_LINE() do {} while(0)
#endif

#ifdef DEBUG
/// @brief Subtract two timespec structures using macros.
/// @note Using clock_gettime(CLOCK_MONOTONIC, &ts) to get the current time.
/// @param result Variable to store the result
/// @param end End time
/// @param start Start time
/// @return Resulting timespec structure representing the difference
#define DBG_TIMESPEC_SUBTRACT(result, end, start)                                       \
do {                                                                                    \
        if ((end).tv_nsec < (start).tv_nsec) {                                          \
                (result).tv_sec  = (end).tv_sec  - (start).tv_sec - 1;                  \
                (result).tv_nsec = 1000000000 + (end).tv_nsec - (start).tv_nsec;        \
        } else {                                                                        \
                (result).tv_sec  = (end).tv_sec  - (start).tv_sec;                      \
                (result).tv_nsec = (end).tv_nsec - (start).tv_nsec;                     \
        }                                                                               \
} while (0)

/// @brief Print the time difference in milliseconds using macros.
/// @param end End time
/// @param start Start time
#define DBG_PRINT_TIME_DIFF_MS(end_var, start_var)                              \
do {                                                                        \
        struct timespec __dbg_diff__;                                           \
        DBG_TIMESPEC_SUBTRACT(__dbg_diff__, (end_var), (start_var));           \
        long long __dbg_diff_ms__ =                                             \
            __dbg_diff__.tv_sec * 1000LL + __dbg_diff__.tv_nsec / 1000000LL;    \
        DMSG("[TIME DIFF] %lld ms (%lld sec %ld nsec)\n",                       \
             __dbg_diff_ms__, __dbg_diff__.tv_sec, __dbg_diff__.tv_nsec);      \
} while (0)
#else
#define DBG_TIMESPEC_SUBTRACT(result, end, start) do {} while(0)
#define DBG_PRINT_TIME_DIFF_MS(end_var, start_var) do {} while(0)
#endif

/// @brief Get the current timestamp in a formatted string
/// @param buf Buffer to store the timestamp
/// @param len Length of the buffer
static inline void dbg_get_timestamp(char *buf, size_t len)
{
        struct timeval tv;
        struct tm tm;
        gettimeofday(&tv, NULL);
        localtime_r(&tv.tv_sec, &tm); ///       Thread-safe localtime
        snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec / 1000);
}

/// @brief Add nanoseconds and milliseconds to a timespec structure representing a specific time.
/// @param base_time The base timespec structure to add to (can be NULL for the current time).
/// @param ms_add Milliseconds to add.
/// @param ns_add Nanoseconds to add.
/// @return Resulting timespec structure representing the time after addition.
static inline struct timespec dbg_timespec_add(const struct timespec *base_time, long ms_add, long ns_add)
{
        struct timespec result;

        // Get current time if no base_time is provided
        if (base_time == NULL) {
                clock_gettime(CLOCK_MONOTONIC, &result);
        } else {
                result = *base_time; // Copy the base time
        }

        // Add milliseconds (convert to nanoseconds)
        result.tv_nsec += ms_add * 1000000;
        result.tv_sec += result.tv_nsec / 1000000000;
        result.tv_nsec %= 1000000000;

        // Add nanoseconds
        result.tv_nsec += ns_add;
        result.tv_sec += result.tv_nsec / 1000000000;
        result.tv_nsec %= 1000000000;

        return result;
}

/// @brief Subtract two timespec structures
/// @note Using clock_gettime(CLOCK_MONOTONIC, &ts) to get the current time.
/// @param end End time
/// @param start Start time
/// @return Resulting timespec structure representing the difference
static inline struct timespec dbg_timespec_subtract(struct timespec end, struct timespec start)
{
        struct timespec result;
        if ((end.tv_nsec - start.tv_nsec) < 0) {
            result.tv_sec = end.tv_sec - start.tv_sec - 1;
            result.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
        } else {
            result.tv_sec = end.tv_sec - start.tv_sec;
            result.tv_nsec = end.tv_nsec - start.tv_nsec;
        }
        return result;
}


/// @brief Print the time difference in milliseconds
/// @param end End time
/// @param start Start time
static inline struct timespec dbg_print_time_diff_ms(struct timespec end, struct timespec start)
{
        struct timespec diff = dbg_timespec_subtract(end, start);
        // The DMSG call here is commented out, but if it were active,
        // it would need to be conditional on DEBUG.
        // long long diff_ms = diff.tv_sec * 1000 + diff.tv_nsec / 1000000;
        // DMSG("[TIME DIFF] %lld ms (%lld sec %ld nsec)\n", diff_ms, diff.tv_sec, diff.tv_nsec);
        // DMSG("[TIME DIFF] %lld ms (%lld sec %ld nsec)\n", diff.tv_sec * 1000 + diff.tv_nsec / 1000000, diff.tv_sec, diff.tv_nsec);
        return diff;
}

#endif