#ifndef __LOG_UI_SENDER_H__
#define __LOG_UI_SENDER_H__

#include <stddef.h>

/// @brief Sender callback for UI log stream delivery.
typedef int (*log_ui_sender_fn)(void *user, int fd, const char *text, size_t text_len);

/// @brief UI log sender contract shared across facade/core/internal sink layers.
typedef struct log_ui_sender_s {
        void *user;
        log_ui_sender_fn send_fn;
} log_ui_sender_t;

#endif /* __LOG_UI_SENDER_H__ */