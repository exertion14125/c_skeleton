#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "util/log/internal/log_sink_udp.h"

/// @brief UDP queue node structure.
struct udp_qnode_s { 
        struct udp_qnode_s *next; 
        size_t len; 
        char *buf; 
};
typedef struct udp_qnode_s udp_qnode_t;

/// @brief UDP context structure.
typedef struct {
        int sock; ///< Socket file descriptor.
        struct sockaddr_in addr; ///< UDP address.
        
        pthread_t pth_udp; ///< UDP thread.
        pthread_mutex_t mt_udp; ///< UDP mutex.
        pthread_cond_t cv_udp; ///< UDP condition variable.
        int stop; ///< Stop flag.

        udp_qnode_t *head; ///< UDP queue head.
        udp_qnode_t *tail; ///< UDP queue tail.
        unsigned q_sz; ///< UDP queue size.
        unsigned q_cap; ///< UDP queue capacity.

        log_sink_stats_t stats; ///< Statistics.
} udp_ctx_t;

/// @brief Push a buffer into the UDP queue.
/// @param c UDP context
/// @param buf Buffer to push.
/// @param len Buffer length.
static void udp_qpush(udp_ctx_t *c, const char *buf, size_t len) 
{
        if (c->q_cap && c->q_sz >= c->q_cap) { 
                c->stats.dropped++; 
                return; 
        }
        udp_qnode_t *n = (udp_qnode_t*)calloc(1, sizeof(*n));
        if (!n) { 
                c->stats.dropped++; 
                return; 
        }
        n->buf = (char*)malloc(len);
        if (!n->buf) { 
                free(n); 
                c->stats.dropped++; 
                return; 
        }
        memcpy(n->buf, buf, len);
        n->len = len;
        if (!c->tail) {
                c->head = c->tail = n;
        } else { 
                c->tail->next=n; 
                c->tail=n; 
        }
        c->q_sz++;
}

/// @brief Pop a buffer from the UDP queue.
/// @param c UDP context
/// @return Pointer to UDP queue node. NULL if queue is empty.
static udp_qnode_t* udp_qpop(udp_ctx_t *c) 
{
        udp_qnode_t *n = c->head;
        if (!n) {
                return NULL;
        }
        c->head = n->next;
        if (!c->head) {
                c->tail = NULL;
        }
        c->q_sz--;
        return n;
}

/// @brief UDP log sink thread.
/// @param arg UDP context.
/// @return NULL.
static void* pth_log_sink_udp(void *arg) 
{
        udp_ctx_t *c = (udp_ctx_t*)arg;
        while (1) {
                usleep(1);
                pthread_mutex_lock(&c->mt_udp);
                while (!c->stop && c->q_sz==0) {
                        pthread_cond_wait(&c->cv_udp, &c->mt_udp);
                }
                if (c->stop && c->q_sz==0) { 
                        pthread_mutex_unlock(&c->mt_udp); 
                        break; 
                }
                udp_qnode_t *n = udp_qpop(c);
                pthread_mutex_unlock(&c->mt_udp);
                if (!n) {
                        continue;
                }

                ssize_t s = sendto(c->sock, n->buf, n->len, 0, (struct sockaddr*)&c->addr, sizeof(c->addr));
                if (s < 0) {
                        c->stats.errors++;
                } else { 
                        c->stats.written++; 
                        c->stats.written_bytes += (uint64_t)s; 
                }
                free(n->buf);
                free(n);
        }
        return NULL;
}

/// @brief Open the UDP log sink.
/// @param ctx UDP context.
/// @return 0 on success, -1 on failure.
static int u_open(void *ctx) 
{
        udp_ctx_t *c=(udp_ctx_t*)ctx;
        if(!c) {
                return -1;
        }
        c->sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (c->sock < 0) {
                return -1;
        }
        pthread_mutex_init(&c->mt_udp, NULL);
        pthread_cond_init(&c->cv_udp, NULL);
        c->stop = 0;

        pthread_attr_t pth_attr;
        pthread_attr_init(&pth_attr);
        pthread_attr_setscope(&pth_attr, PTHREAD_SCOPE_SYSTEM);
        pthread_attr_setstacksize(&pth_attr, 64 * 1024); // stack_size를 적절히 설정
        
        if (pthread_create(&c->pth_udp, &pth_attr, pth_log_sink_udp, c) != 0) {
                return -1;
        }
        return 0;
}

/// @brief Close the UDP log sink.
/// @param ctx UDP context.
static void u_close(void *ctx) 
{
        udp_ctx_t *c=(udp_ctx_t*)ctx;
        if(!c) {
                return;
        }
        pthread_mutex_lock(&c->mt_udp);
        c->stop = 1;
        pthread_cond_broadcast(&c->cv_udp);
        pthread_mutex_unlock(&c->mt_udp);
        pthread_join(c->pth_udp, NULL);
        if (c->sock >= 0) {
                close(c->sock);
        }

        udp_qnode_t *n;
        while ((n=udp_qpop(c)) != NULL) { 
                free(n->buf); 
                free(n); 
        }
        pthread_cond_destroy(&c->cv_udp);
        pthread_mutex_destroy(&c->mt_udp);
}

/// @brief Write a buffer to the UDP log sink.
/// @param ctx UDP context.
/// @param buf Buffer to write.
/// @param len Buffer length.
/// @return 0 on success, -1 on failure.
static int u_write(void *ctx, const char *buf, size_t len) 
{
        udp_ctx_t *c=(udp_ctx_t*)ctx;
        if(!c) {
                return -1;
        }
        pthread_mutex_lock(&c->mt_udp);
        udp_qpush(c, buf, len);
        pthread_cond_signal(&c->cv_udp);
        pthread_mutex_unlock(&c->mt_udp);
        return 0;
}

/// @brief Flush the UDP log sink.
/// @param ctx UDP context.
static void u_flush(void *ctx) 
{ 
        (void)ctx; 
}

/// @brief Get statistics of the UDP log sink.
/// @param ctx UDP context.
/// @param out Output statistics.
/// @return 0 on success, -1 on failure.
static int u_get_stats(void *ctx, log_sink_stats_t *out) 
{
        udp_ctx_t *c=(udp_ctx_t*)ctx;
        if (!c || !out) {
                return -1;
        }
        *out = c->stats;
        return 0;
}

/// @brief Destroy the UDP log sink.
/// @param s UDP log sink.
static void u_destroy(log_sink_t *s) 
{
        if (!s) {
                return;
        }
        udp_ctx_t *c = (udp_ctx_t*)s->ctx;
        if (c) { 
                u_close(c); 
                free(c); 
        }
        free(s);
}

/// @brief 
static const log_sink_ops_t g_ops = {
        .open = u_open,
        .close = u_close,
        .write = u_write,
        .flush = u_flush,
        .rotate = NULL,
        .get_size = NULL,
        .get_stats = u_get_stats,
        .get_first_remain_done = NULL,
};

/// @brief Reconfigure the UDP log sink.
/// @param s UDP log sink.
/// @param cfg New configuration.
/// @return 0 on success, -1 on failure.
int log_sink_udp_reconfigure(log_sink_t *s, const log_sink_udp_cfg_t *cfg) 
{
        if (!s || !cfg) {
                return -1;
        }
        udp_ctx_t *c = (udp_ctx_t*)s->ctx;
        if (!c) return -1;

        pthread_mutex_lock(&c->mt_udp);
        if (cfg->port) c->addr.sin_port = htons(cfg->port);
        if (cfg->host && cfg->host[0]) {
                inet_aton(cfg->host, &c->addr.sin_addr);
        }
        pthread_mutex_unlock(&c->mt_udp);
        return 0;
}

/// @brief Create a UDP log sink.
/// @param cfg UDP log sink configuration.
/// @return Pointer to UDP log sink. NULL on failure.
log_sink_t* log_sink_udp_create(const log_sink_udp_cfg_t *cfg) 
{
        log_sink_t *s = (log_sink_t*)calloc(1,sizeof(*s));
        udp_ctx_t *c = (udp_ctx_t*)calloc(1,sizeof(*c));
        if(!s||!c) { 
                free(s); 
                free(c); 
                return NULL; 
        }
        memset(&c->addr, 0, sizeof(c->addr));
        c->addr.sin_family = AF_INET;
        c->addr.sin_port = htons((cfg && cfg->port) ? cfg->port : LOG_SINK_UDP_CFG_DEF_PORT);
        const char *host = (cfg && cfg->host) ? cfg->host : LOG_SINK_UDP_CFG_DEF_IP;
        inet_aton(host, &c->addr.sin_addr);
        c->q_cap = (cfg && cfg->q_cap) ? cfg->q_cap : 256;
        c->sock = -1;

        s->ops = &g_ops; 
        s->ctx = c; 
        s->refcnt = 1; 
        s->destroy = u_destroy;
        return s;
}
