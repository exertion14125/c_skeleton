#ifndef __LOG_SINK_H__
#define __LOG_SINK_H__
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/// @brief Log sink statistics structure.
struct log_sink_stats_s {
        uint64_t written; ///< Number of written log entries.
        uint64_t dropped; ///< Number of dropped log entries.
        uint64_t errors; ///< Number of write errors.
        uint64_t written_bytes; ///< Number of written bytes.
};
typedef struct log_sink_stats_s log_sink_stats_t;

/// @brief Log sink operations structure.
struct log_sink_ops_s {
        int  (*open)(void *ctx); ///< Sink resource open function pointer.
        void (*close)(void *ctx); ///< Sink resource close function pointer.
        int  (*write)(void *ctx, const char *buf, size_t len); ///< Sink resource write function pointer.
        void (*flush)(void *ctx); ///< Sink resource flush function pointer.
        //===== Optional =====//
        int  (*rotate)(void *ctx, bool remain_first, char *rotated_path, size_t rotated_sz); ///< Sink resource rotate function pointer for file sink.
        int  (*get_size)(void *ctx, size_t *out_size); ///< Sink resource get size function pointer for file sink.
        int  (*get_first_remain_done)(void *ctx, bool *out_remain_first_done); ///< Sink resource get size function pointer for file sink.
        int  (*get_stats)(void *ctx, log_sink_stats_t *out_stats);
};
typedef struct log_sink_ops_s log_sink_ops_t;

/// @brief Log sink structure.
struct log_sink_s {
        const log_sink_ops_t *ops;
        void *ctx;
        volatile int refcnt; ///< Reference count for hot-swap safety. Initialized to 1 on allocation.
        void (*destroy)(struct log_sink_s *s);
};
typedef struct log_sink_s log_sink_t;

extern log_sink_t* log_sink_ref(log_sink_t *sink);
extern void log_sink_unref(log_sink_t *sink);

#endif /* __LOG_SINK_H__ */