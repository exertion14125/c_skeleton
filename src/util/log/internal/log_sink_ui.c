#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <pthread.h>
#include <alloca.h>

#include "util/log/internal/log_sink_ui.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#ifndef LOG_SINK_UI_DEFAULT_BACKLOG
#define LOG_SINK_UI_DEFAULT_BACKLOG 1024
#endif

#ifndef LOG_SINK_UI_DEFAULT_LINE_MAX
#define LOG_SINK_UI_DEFAULT_LINE_MAX 256
#endif

struct log_ui_ring_s {
        char *mem; ///< slots * line_max bytes
        uint64_t *seq; ///< slots (sequence numbers of each slot; 0 if empty)
        uint32_t slots; ///< Number of slots.
        uint32_t line_max; ///< Maximum line length.
        
        uint32_t widx; ///< next write index
        uint32_t depth; ///< <=slots

        uint64_t wseq; ///< monotonically increasing write sequence number.
};
typedef struct log_ui_ring_s log_ui_ring_t;

/// @brief Log sink UI context structure.
typedef struct log_sink_ui_ctx_s {
        log_sink_ui_cfg_t cfg; ///< Configuration.
        int uds_fd; ///< -1 is not UI connected.
        int has_subscriber; ///< Has UI subscriber or not.

        log_ui_ring_t log_ui_ring;
        pthread_mutex_t ring_mt;

        log_sink_stats_t stats; ///< Statistics.
} log_sink_ui_ctx_t;


/// @brief Initialize the UI ring buffer.
/// @param ring Pointer to UI ring buffer.
/// @param slots Number of slots.
/// @param line_max Maximum line length.
/// @return 0 on success, -1 on failure.
static int alloc_log_ui_ring(log_ui_ring_t *ring, uint32_t slots, uint32_t line_max)
{
        if (!ring ) {
                return -1;
        }
        if (slots == 0) {
                slots = LOG_SINK_UI_DEFAULT_BACKLOG;
        }
        if (line_max == 0) {
                line_max = LOG_SINK_UI_DEFAULT_LINE_MAX;
        }
        ring->mem = (char*)calloc((size_t)slots, (size_t)line_max);
        if (!ring->mem) {
                return -1;
        }
        ring->seq = (uint64_t*)calloc((size_t)slots, sizeof(uint64_t));
        if (!ring->seq) {
                free(ring->mem);
                ring->mem = NULL;
                return -1;
        }
        ring->slots = slots;
        ring->line_max = line_max;
        ring->widx = 0;
        ring->depth = 0;
        return 0;
}

/// @brief Destroy the UI ring buffer.
/// @param ring Pointer to UI ring buffer.
static void destroy_log_ui_ring(log_ui_ring_t *ring)
{
        if (!ring) {
                return;
        }
        if (ring->mem) {
                free(ring->mem);
                ring->mem = NULL;
        }
        if (ring->seq) {
                free(ring->seq);
                ring->seq = NULL;
        }
        ring->slots = 0;
        ring->line_max = 0;
        ring->widx = 0;
        ring->depth = 0;
}

/// @brief Push a line to the UI ring buffer.
/// @param ring Pointer to UI ring buffer.
/// @param line Line to push.
/// @param len Length of the line.
static void push_log_ui_ring(log_ui_ring_t *ring, const char *line, size_t len)
{
        if (!ring || !ring->mem || !line || len == 0) {
                return;
        }
        
        uint32_t idx = ring->widx;
        char *dst = ring->mem + (size_t)idx * (size_t)ring->line_max;

        size_t n = len;
        if (n>= ring->line_max) {
                n = ring->line_max - 1;
        }
        memcpy(dst, line, n);
        dst[n] = '\0';
        
        ring->wseq++;
        ring->seq[idx] = ring->wseq;

        ring->widx = (idx + 1) % ring->slots;
        if (ring->depth < ring->slots) {
                ring->depth++;
        }


}

/// @brief Get the oldest index in the UI ring buffer.
/// @param ring Pointer to UI ring buffer.
/// @return Oldest index.
static uint32_t get_log_ui_ring_oldest_idx(log_ui_ring_t *ring)
{
        if (!ring || ring->depth == 0) {
                return 0;
        }
        uint32_t widx = ring->widx;
        uint32_t depth = ring->depth;
        if (depth == 0) {
                return widx;
        }
        if (widx >= depth) {
                return widx - depth;
        }
        uint32_t idx = ring->slots -(depth - widx);
        return idx;
}

/// @brief Check if the send error is fatal.
/// @param err Error code.
/// @return 1 if fatal, 0 otherwise.
static int is_fatal_send_err(int err)
{
        return (err == EPIPE || err == ECONNRESET || err == ENOTCONN || err == EBADF);
}

/// @brief Send all data in the buffer.
/// @param fd File descriptor.
/// @param buf Buffer to send.
/// @param len Length of the buffer.
/// @return 0 on success, negative errno on failure.
static int send_all_buffer(int fd, const char *buf, size_t len)
{
        size_t off = 0;
        while (off < len) {
                ssize_t n = send(fd, buf + off, len - off, MSG_NOSIGNAL);
                if (n > 0) {
                        off += (size_t)n;
                        continue;
                } 
                if (n == 0) {
                        return -EPIPE;
                } 
                if (n < 0) {
                        if (errno == EINTR) {
                                continue;
                        }
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                return -EAGAIN;
                        }
                        return -errno;
                }
        }
        return 0;
}


/// @brief Open the UI log sink.
/// @param ctx UI context.
/// @return 0 on success, negative errno on failure.
static int ui_open(void *ctx)
{
        log_sink_ui_ctx_t *c = (log_sink_ui_ctx_t*)ctx;
        if (!c) {
                return -EINVAL;
        }
        return 0;
}

/// @brief Close the UI log sink.
/// @param ctx UI context.
static void ui_close(void *ctx)
{
        log_sink_ui_ctx_t *c = (log_sink_ui_ctx_t*)ctx;
        if (!c) {
                return;
        }
        pthread_mutex_lock(&c->ring_mt);
        c->uds_fd = -1;
        c->has_subscriber = 0;
        pthread_mutex_unlock(&c->ring_mt);
}

/// @brief Write a buffer to the UI log sink.
/// @param ctx UI context.
/// @param buf Buffer to write.
/// @param len Buffer length.
/// @return 0 on success, negative errno on failure.
static int ui_write(void *ctx, const char *buf, size_t len)
{
        log_sink_ui_ctx_t *c = (log_sink_ui_ctx_t*)ctx;
        if (!c || !buf) {
                return -1;
        }

        char *tmp = NULL;
        size_t n = len;
        if (n == 0) {
                return 0;
        }
        // Keep at most line_max -1 characters + null terminator
        if (n >= c->cfg.line_max) {
                n = c->cfg.line_max - 1;
        }

        tmp = (char*)alloca((size_t)c->log_ui_ring.line_max); // stack allocation and auto free(out of scope)
        memcpy(tmp, buf, n);
        tmp[n] = '\0';

        pthread_mutex_lock(&c->ring_mt);
        // policy check.
        if (!c->has_subscriber && !c->cfg.enable_when_no_ui) {
                // No subscriber and not enabled when no UI
                c->stats.dropped++;
                pthread_mutex_unlock(&c->ring_mt);
                return 0;
        }
        // Push to ring buffer
        push_log_ui_ring(&c->log_ui_ring, tmp, n);
        
        c->stats.written++;
        c->stats.written_bytes += (uint64_t)n;

        int fd = c->uds_fd;
        bool do_stream = (c->cfg.enable_uds_notify && c->has_subscriber && fd >= 0);
        // printf("UI sink: do_stream=%d has_subscriber=%d uds_fd=%d\n", do_stream ? 1 : 0, c->has_subscriber ? 1 : 0, fd);
        pthread_mutex_unlock(&c->ring_mt);

        if (!do_stream) {
                return 0;
        }

        int rc = send_all_buffer(fd, tmp, strnlen(tmp, (size_t) c->log_ui_ring.line_max));
        if (rc == 0) {
                return 0;
        }
        // Handle send error
        pthread_mutex_lock(&c->ring_mt);
        c->stats.errors++;

        if (rc == -EAGAIN) {
                // Non-fatal error, try later
                c->stats.dropped++;
                pthread_mutex_unlock(&c->ring_mt);
                return 0;
        }

        if (is_fatal_send_err(-rc)) {
                // Fatal error, clear subscriber
                c->has_subscriber = 0;
                c->uds_fd = -1;
        }
        pthread_mutex_unlock(&c->ring_mt);
        return 0;
}

/// @brief Get statistics of the UI log sink.
/// @param ctx UI context.
/// @param out_stats Output statistics.
/// @return 0 on success, negative errno on failure.
static int ui_get_stats(void *ctx, log_sink_stats_t *out_stats)
{
        log_sink_ui_ctx_t *c = (log_sink_ui_ctx_t*)ctx;
        if (!c || !out_stats) return -EINVAL;
        pthread_mutex_lock(&c->ring_mt);
        *out_stats = c->stats;
        pthread_mutex_unlock(&c->ring_mt);
        return 0;
}

/// @brief UI log sink operations.
static const log_sink_ops_t g_ui_ops = {
        ui_open,
        ui_close,
        ui_write,
        NULL,
        NULL,
        NULL,
        NULL,
        ui_get_stats
};

/// @brief Destroy the UI log sink.
/// @param s UI log sink.
static void ui_sink_destroy(log_sink_t *s)
{
        if (!s) return;
        if (s->ctx) {
                log_sink_ui_ctx_t *c = (log_sink_ui_ctx_t*)s->ctx;
                ui_close(c);
                destroy_log_ui_ring(&c->log_ui_ring);
                pthread_mutex_destroy(&c->ring_mt);
                free(c);
        }
        free(s);
}

/// @brief Create a UI log sink.
/// @param cfg UI log sink configuration.
/// @return Pointer to UI log sink. NULL on failure.
log_sink_t* log_sink_ui_create(const log_sink_ui_cfg_t *cfg)
{
        if (!cfg) { 
                errno = EINVAL; 
                return NULL; 
        }

        log_sink_t *s = (log_sink_t*)calloc(1, sizeof(*s));
        if (!s) {
                return NULL;
        }

        log_sink_ui_ctx_t *c = (log_sink_ui_ctx_t*)calloc(1, sizeof(*c));
        if (!c) { 
                free(s); 
                return NULL;
        }

        c->cfg = *cfg;
        if (c->cfg.backlog_slot == 0) {
                c->cfg.backlog_slot = LOG_SINK_UI_DEFAULT_BACKLOG;
        }
        if (c->cfg.line_max == 0) {
                c->cfg.line_max = LOG_SINK_UI_DEFAULT_LINE_MAX;
        }

        pthread_mutex_init(&c->ring_mt, NULL);

        c->uds_fd = -1;
        c->has_subscriber = 0;
        memset(&c->stats, 0, sizeof(c->stats));

        int rc = alloc_log_ui_ring(&c->log_ui_ring, c->cfg.backlog_slot, c->cfg.line_max);
        if (rc != 0) {
                pthread_mutex_destroy(&c->ring_mt);
                free(c);
                free(s);
                errno = rc;
                return NULL;
        }

        s->ops = &g_ui_ops;
        s->ctx = c;
        s->refcnt = 1;
        s->destroy = ui_sink_destroy;

        return s;
}

/// @brief
/// @param sink 
/// @param uds_fd 
/// @return 
int log_sink_ui_attach_fd(log_sink_t *sink, int uds_fd, uint64_t last_seen_wseq)
{
        if (!sink || !sink->ctx) {
                return -EINVAL;
        }
        if (uds_fd < 0) {
                return -EINVAL;
        }
        log_sink_ui_ctx_t *c = (log_sink_ui_ctx_t*)sink->ctx;

        pthread_mutex_lock(&c->ring_mt);
        c->uds_fd = uds_fd;
        c->has_subscriber = 1;

        uint32_t depth = c->log_ui_ring.depth;
        uint32_t slots = c->log_ui_ring.slots;
        uint32_t line_max = c->log_ui_ring.line_max;
        uint32_t start = get_log_ui_ring_oldest_idx(&c->log_ui_ring);

        //==== Copy (line and sequence) to temporary buffer
        char *lines = NULL;
        uint64_t *seqs = NULL;
        if (depth > 0) {
                lines = (char*)calloc((size_t)depth, (size_t)line_max);
                seqs = (uint64_t*)calloc((size_t)depth, sizeof(uint64_t));
                if (!lines || !seqs) {
                        c->stats.errors++;
                } else {
                        for (uint32_t i = 0; i < depth; i++) {
                                uint32_t idx = (start + i) % slots;
                                char *src = c->log_ui_ring.mem + (size_t)idx * (size_t)line_max;
                                char *dst = lines + (size_t)i * (size_t)line_max;
                                strncpy(dst, src, line_max-1);
                                dst[line_max - 1] = '\0';

                                seqs[i] = c->log_ui_ring.seq[idx];
                        }
                        
                }

        }
        pthread_mutex_unlock(&c->ring_mt);

        //=== flush backlog to the new subscriber
        if (lines && seqs && c->cfg.enable_uds_notify) {
                for (uint32_t i = 0; i < depth; i++) {
                        uint64_t sseq = seqs[i];
                        if (sseq == 0) { // empty slot
                                continue;
                        }
                        if (sseq <= last_seen_wseq) { // already seen.
                                continue;
                        }

                        const char *line = lines + (size_t)i * (size_t)line_max;
                        size_t ln = strnlen(line, (size_t)line_max);
                        if (ln == 0 ) {
                                continue;
                        }
                        int rc = send_all_buffer(uds_fd, line, ln);
                        if (rc == 0) {
                                continue;
                        }
                        // Handle send error
                        pthread_mutex_lock(&c->ring_mt);
                        c->stats.errors++;
                        if (rc == - EAGAIN) {
                                /// try later.
                                pthread_mutex_unlock(&c->ring_mt);
                                break;
                        }

                        if (is_fatal_send_err(-rc)) {
                                // Fatal error, clear subscriber
                                c->has_subscriber = 0;
                                c->uds_fd = -1;
                        }
                        pthread_mutex_unlock(&c->ring_mt);
                        break;
                }
        }
        if (lines) {
                free(lines);
        }
        if (seqs) {
                free(seqs);
        }
        return 0;
}


/// @brief Set UDS file descriptor.
/// @param sink UI log sink.
/// @param uds_fd UDS file descriptor. -1 if no UI connected.
/// @return 0 on success, negative errno on failure.
int log_sink_ui_detach_fd(log_sink_t *sink, int uds_fd)
{
        (void) uds_fd;
        if (!sink || !sink->ctx) {
                return -EINVAL;
        }
        log_sink_ui_ctx_t *c = (log_sink_ui_ctx_t*)sink->ctx;

        pthread_mutex_lock(&c->ring_mt);
        c->uds_fd = -1;
        c->has_subscriber = 0;
        pthread_mutex_unlock(&c->ring_mt);
        return 0;
}
