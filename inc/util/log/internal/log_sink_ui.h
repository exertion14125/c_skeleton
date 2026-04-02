#ifndef __LOG_SINK_UI_H__
#define __LOG_SINK_UI_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "log_sink.h"

#define UI_EVT_LOG 'L' ///< UI event log type.

/// @brief Log sink UI configuration structure.
typedef struct log_sink_ui_cfg_s {
        // char uds_path[256]; ///< UDS socket path.
        bool enable_uds_notify; ///< Enable UDS.
        bool enable_when_no_ui; ///< Enable logging even when no UI is connected.
        uint32_t backlog_slot; ///< Backlog slot count for UI log sink.
        uint32_t line_max; ///< Maximum line length for UI log sink.
} log_sink_ui_cfg_t;

extern log_sink_t* log_sink_ui_create(const log_sink_ui_cfg_t *cfg);

extern int log_sink_ui_attach_fd(log_sink_t *sink, int uds_fd, uint64_t last_seen_wseq);
extern int log_sink_ui_detach_fd(log_sink_t *sink, int uds_fd);

// sender callback for UI log sink to send log line to UI subscriber.
typedef int (*log_ui_stream_sender_fn)(void *user, int fd, const char *text, size_t text_len);

/// @brief Log UI sender structure.
typedef struct log_ui_sender_s {
        void *user;
        log_ui_stream_sender_fn send_fn;
} log_ui_stream_sender_t;

extern int log_sink_ui_set_sender(log_sink_t *sink, const log_ui_stream_sender_t *sender);
#endif /* __LOG_SINK_UI_H__ */