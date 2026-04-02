#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>

#include "util/log/internal/log_types.h"
#include "util/log/internal/log_policy.h"
#include "util/log/internal/log_sink.h"
#include "util/log/internal/log_sink_file.h"
#include "util/log/internal/log_sink_udp.h"
#include "util/log/internal/log_sink_ui.h"
#include "util/log/internal/log_core.h"

/// @brief Log message structure.
struct log_msg_s {
        uint64_t ts_ms; //< Logging time.
        log_level_t lvl;//< Logging level.
        char tag[32];   //< Logging message tag.
        char text[1];    //< Logging message.  FAM. @todo char msg[];
};
typedef struct log_msg_s log_msg_t;

/// @brief Log message linked list node structure.
struct log_node_s {
        struct log_node_s* next; //< Logging message linked list next node.
        log_msg_t *msg; //< Logging message.
};
typedef struct log_node_s log_node_t;

/// @brief Log message queue structure.
struct log_queue_s {
        log_node_t *head; //< Log linked list head node.
        log_node_t *tail; //< Log linked list tail node.
        size_t sz; //< Log linked list node size.
        size_t cap; //< Log linked list node capacity.
};
typedef struct log_queue_s log_queue_t;


// /// @brief Rotate event structure.
// struct rotate_ev_s {
//         struct rotate_ev_s *next;
//         char rotated_path[256];
//         int partial;
//         int failed;
// };
// typedef struct rotate_ev_s rotate_ev_t;

// /// @brief Rotate event queue structure.
// struct rotate_queue_s {
//         rotate_ev_t *head; //< Rotate linked list head node.
//         rotate_ev_t *tail; //< Rotate linked list tail node.
//         size_t sz; //< Rotate linked list size;
//         size_t cap; //< Rotate linked list capacity.
// };
// typedef struct rotate_queue_s rotate_queue_t;

/// @brief Log core configuration structure.
struct log_cfg_s {
        /// Config struct info.
        uint32_t size; ///< Size of this structure.
        uint32_t version; ///< Version of this structure.
        /// General configuration.
        log_level_t level; ///< log level filter.
        size_t queue_size; ///< log queue size.
        size_t max_msg; ///< log max message size.
        struct {
                bool enable; ///< enable
                char fpath[256]; ///< log file path.
                bool remain_first; ///< log first file remain or not. (true:remain, false: not remain)
                size_t max_file_size; ///< Each log file max size.
                size_t max_file_count; ///< log rotation count.
                int flush_line_default; ///< log flush line count. (0:never, 1:every line, N:every N lines)
                int fsync_line_default; ///< log fsync line count. (0:never, 1:every line, N:every N lines)
        } file;
        struct {
                bool enable; ///< UDP sink enable or not.
                char udp_ip[64]; ///< UDP target IP.
                uint16_t udp_port; ///< UDP target port.
        } udp;
        struct {
                bool enable; ///< UI sink enable or not.
                char uds_path[256]; ///< UI sink Unix domain socket path. // Not used in sink, only for config record. Manage is ui_mgr/ra_ui_uds.
                bool enable_no_ui; ///< UI sink enable when no UI.
                bool enable_uds_notify; ///< UI sink UDS notify enable or not.
                uint32_t backlog_slot; ///< UI sink backlog slot count.
        } ui;
};
typedef struct log_cfg_s log_cfg_t;

/// @brief Log core structure.
struct log_core_s {
        log_cfg_t cfg; ///< log core configuration.
        log_policy_t  *policy; ///< log policy.
        log_sink_t    *sink_file; ///< log sinks - file sink.
        log_sink_t    *sink_net; ///< log sinks - network sink (UDP).
        log_sink_t    *sink_ui; ///< log sinks - UI sink.
        int ui_fd; ///< UI sink UDS fd.

        log_ui_sender_t ui_sender; ///< registered ui sender callback
        int ui_sender_valid; ///< ui sender registration flag

        // log_stats_t    stats;
//====== log stats
        bool inited; ///< log core initialized or not. (set opt and init_log first only once)
        bool stop_log; ///< log thread stop flag.
        // bool stop_rotate; ///< rotate thread stop flag.
//====== log runtime resources
        log_queue_t log; ///< log message queue.
        pthread_t pth_log; ///< log thread.
        pthread_mutex_t mt_log; ///< log mutex.
        pthread_cond_t cv_log; ///< log condition variable.

        pthread_mutex_t mt_sink; ///< log sink mutex.

        // rotate_queue_t rotate;
        // pthread_mutex_t mt_rotate;
        // pthread_cond_t cv_rotate;
};
typedef struct log_core_s log_core_t;

//====== Internal function implementation =======

/// @brief Get current time in milliseconds.
/// @return Current time in milliseconds.
static uint64_t now_ms(void) 
{
    struct timeval tv; 
    gettimeofday(&tv,NULL);
    return (uint64_t)tv.tv_sec*1000ull + (uint64_t)(tv.tv_usec/1000);
}

/// @brief Initialize log message queue.
/// @param q Pointer to log message queue.
/// @param cap Capacity of log message queue.
static void log_queue_init(log_queue_t *q, size_t cap)
{ 
        memset(q, 0, sizeof(*q)); 
        q->cap = cap; 
}

/// @brief Push log message to queue.
/// @param q Pointer to log message queue.
/// @param m Pointer to log message.
/// @return 0 on success, -1 on failure.
static int log_queue_push(log_queue_t *q, log_msg_t *m)
{
        if (q->cap && q->sz>=q->cap) {
                return -1;
        }
        log_node_t *n = (log_node_t*)calloc(1,sizeof(*n));
        if (!n) {
                return -1;
        }
        n->msg = m;
        if (!q->tail) {
                q->head = q->tail=n;
        } else { 
                q->tail->next = n; 
                q->tail = n; 
        }
        q->sz++;
        return 0;
}

/// @brief Pop log message from queue.
/// @param q Pointer to log message queue.
/// @return Pointer to log message. NULL if queue is empty.
static log_msg_t* log_queue_pop(log_queue_t *q)
{
        if (!q->head) {
                return NULL;
        }
        log_node_t *n = q->head;
        q->head = n->next;
        if (!q->head) {
                q->tail = NULL;
        }
        q->sz--;
        log_msg_t *m = n->msg;
        free(n);
        return m;
}

// @brief Add log sink.
/// @param sink_mask Add sink mask.
/// @param sink Pointer to log sink.
/// @return 0 on success, -1 on failure.
static int  add_log_sink(log_core_t *core, unsigned sink_mask, log_sink_t *sink)
{
        if (!core) {
                return -1;
        }
        if (!sink || !sink->ops) {
                return -1;
        }
        sink->refcnt = 1;
        if (sink->ops->open) {
                if (sink->ops->open(sink->ctx) != 0) return -1;
        }

        pthread_mutex_lock(&core->mt_sink);
        log_sink_t **slot = NULL;
        if (sink_mask & LOG_SINK_FILE) {
                slot = &core->sink_file;
        } else if (sink_mask & LOG_SINK_UDP) {
                slot = &core->sink_net;
        } else if (sink_mask & LOG_SINK_UI) {
                slot = &core->sink_ui;
        } else { 
                pthread_mutex_unlock(&core->mt_sink); 
                return -1; 
        }

        log_sink_t *old = *slot;
        *slot = log_sink_ref(sink);
        pthread_mutex_unlock(&core->mt_sink);

        log_sink_unref(old);
        return 0;
}

/// @brief Remove log sink.
/// @param sink_mask Remove sink mask.
/// @return 0 on success.
static int  remove_log_sink(log_core_t *core, unsigned sink_mask)
{
        if (!core) {
                return -1;
        }
        pthread_mutex_lock(&core->mt_sink);
        log_sink_t *old = NULL;
        if (sink_mask & LOG_SINK_FILE) { 
                old = core->sink_file; 
                core->sink_file = NULL; 
        } else if (sink_mask & LOG_SINK_UDP) { 
                old=core->sink_net; 
                core->sink_net=NULL; 
        } else if (sink_mask & LOG_SINK_UI) { 
                old=core->sink_ui; 
                core->sink_ui=NULL; 
        }
        pthread_mutex_unlock(&core->mt_sink);
        log_sink_unref(old);
        return 0;
}

/// @brief Get log sink by sink mask.
/// @param sink_mask Sink mask.
/// @return Pointer to log sink. NULL if not found.
static log_sink_t* get_log_sink(log_core_t *core, unsigned sink_mask) 
{
        if (!core) {
                return NULL;
        }
        pthread_mutex_lock(&core->mt_sink);
        log_sink_t *s = NULL;
        if (sink_mask & LOG_SINK_FILE) s = log_sink_ref(core->sink_file);
        else if (sink_mask & LOG_SINK_UDP) s = log_sink_ref(core->sink_net);
        else if (sink_mask & LOG_SINK_UI) s = log_sink_ref(core->sink_ui);
        pthread_mutex_unlock(&core->mt_sink);
        return s;
}

/// @brief Apply file sink configuration.
/// @param core Pointer to log core.
/// @param in Pointer to input configuration.
/// @return 0 on success, -1 on failure.
static int apply_file_sink(log_core_t *core, const log_cfg_in_t *in)
{
        if (!core || !in) {
                return -1;
        }
        int enable = (in->file_fpath && in->file_fpath[0] != '\0');
        if (!enable) {
                core->cfg.file.enable = false;
                remove_log_sink(core, LOG_SINK_FILE);
                return 0;
        }
        log_sink_file_cfg_t file_cfg;
        memset(&file_cfg, 0, sizeof(file_cfg));
        snprintf(file_cfg.path, sizeof(in->file_fpath), "%s", in->file_fpath);
        file_cfg.max_files = in->max_file_count ? in->max_file_count : DEFAULT_LOG_FILE_MAX_COUNT;
        file_cfg.max_size = in->max_file_size ? in->max_file_size : DEFAULT_LOG_FILE_MAX_SIZE;
        file_cfg.flush_lines = in->flush_line;;
        file_cfg.fsync_lines = in->fsync_line;
        log_sink_t *sink = log_sink_file_create((log_sink_file_cfg_t*)&file_cfg);
        if (!sink) {
                return -1;
        }
        int rc = add_log_sink(core, LOG_SINK_FILE, sink);
        if (rc != 0) {
                return rc;
        }
        core->cfg.file.enable = true;
        core->cfg.file.max_file_count = file_cfg.max_files;
        core->cfg.file.max_file_size = file_cfg.max_size;
        core->cfg.file.flush_line_default = file_cfg.flush_lines;
        core->cfg.file.fsync_line_default = file_cfg.fsync_lines;
        core->cfg.file.remain_first = in->remain_first;
        snprintf(core->cfg.file.fpath, sizeof(core->cfg.file.fpath), "%s", in->file_fpath);
        // printf("File sink applied: fpath=%s, max_file_size=%zu, max_file_count=%zu, flush_line=%d, fsync_line=%d\n",
        //         core->cfg.file.fpath,
        //         core->cfg.file.max_file_size,
        //         core->cfg.file.max_file_count,
        //         core->cfg.file.flush_line_default,
        //         core->cfg.file.fsync_line_default);
        uint32_t mask = get_log_policy_sink_mask(core->policy);
        mask |= LOG_SINK_FILE;
        set_log_policy_sink_mask(core->policy, mask);
        set_log_policy_file_max_files(core->policy, core->cfg.file.max_file_count);
        set_log_policy_file_rotate_size(core->policy, core->cfg.file.max_file_size);
        set_log_policy_file_remain_first(core->policy, core->cfg.file.remain_first);
        return 0;
}

/// @brief Apply UDP sink configuration.
/// @param core Pointer to log core.
/// @param in Pointer to input configuration.
/// @return 0 on success, -1 on failure.
static int apply_udp_sink(log_core_t *core, const log_cfg_in_t *in)
{
        if (!core || !in) {
                return -1;
        }

        int enable = (in->udp_ip && in->udp_ip[0] != '\0' && in->udp_port != 0);
        if (!enable) {
                core->cfg.udp.enable = false;
                remove_log_sink(core, LOG_SINK_UDP);
                return 0;
        }
        log_sink_udp_cfg_t udp_cfg;
        memset(&udp_cfg, 0, sizeof(udp_cfg));
        snprintf(udp_cfg.host, sizeof(in->udp_ip), "%s", in->udp_ip);
        udp_cfg.port = in->udp_port;
        log_sink_t *sink = log_sink_udp_create((log_sink_udp_cfg_t*)&udp_cfg);
        if (!sink) {
                return -1;
        }
        int rc = add_log_sink(core, LOG_SINK_UDP, sink);
        if (rc != 0) {
                return rc;
        }
        core->cfg.udp.enable = true;
        core->cfg.udp.udp_port = in->udp_port;
        snprintf(core->cfg.udp.udp_ip, sizeof(core->cfg.udp.udp_ip), "%s", in->udp_ip);
        // printf("UDP sink applied: ip=%s, port=%u\n", core->cfg.udp.udp_ip, core->cfg.udp.udp_port);
        uint32_t mask = get_log_policy_sink_mask(core->policy);
        mask |= LOG_SINK_UDP;
        set_log_policy_sink_mask(core->policy, mask);
        return 0;
}

/// @brief Apply UI sink configuration.
/// @param core Pointer to log core.
/// @param in Pointer to input configuration.
/// @return 0 on success, -1 on failure.
static int apply_ui_sink(log_core_t *core, const log_cfg_in_t *in)
{
        if (!core || !in) {
                return -1;
        }
        int enable = in->ui_enable;
        if (!enable) {
                core->ui_fd = -1;
                core->cfg.ui.enable = false;
                remove_log_sink(core, LOG_SINK_UI);
                // core->cfg.ui.uds_path[0] = '\0';
                core->cfg.ui.enable_no_ui = false;
                core->cfg.ui.enable_uds_notify = false;
                // core->cfg.ui.backlog_slot = 0;
                return 0;
        }
        log_sink_ui_cfg_t ui_cfg;
        memset(&ui_cfg, 0, sizeof(ui_cfg));
        // snprintf(ui_cfg.uds_path, sizeof(ui_cfg.uds_path), "%s", in->uds_path ? in->uds_path : "");
        ui_cfg.enable_uds_notify = in->uds_notify;
        ui_cfg.enable_when_no_ui = in->enable_no_ui;
        // ui_cfg.backlog_slot = in->backlog_store;
        log_sink_t *sink = log_sink_ui_create((log_sink_ui_cfg_t*)&ui_cfg);
        if (!sink) {
                return -1;
        }
        int rc = add_log_sink(core, LOG_SINK_UI, sink);
        if (rc != 0) {
                return rc;
        }
        core->cfg.ui.enable = true;
        core->cfg.ui.enable_no_ui = in->enable_no_ui;
        core->cfg.ui.enable_uds_notify = in->uds_notify;
        // core->cfg.ui.backlog_slot = in->backlog_store;
        // snprintf(core->cfg.ui.uds_path, sizeof(core->cfg.ui.uds_path), "%s", in->uds_path ? in->uds_path : "");
        // printf("UI sink applied: enable_no_ui=%d, enable_uds_notify=%d, backlog_slot=%u\n",
        //         // core->cfg.ui.uds_path,
        //         core->cfg.ui.enable_no_ui,
        //         core->cfg.ui.enable_uds_notify,
        //         core->cfg.ui.backlog_slot);
        uint32_t mask = get_log_policy_sink_mask(core->policy);
        mask |= LOG_SINK_UI;
        set_log_policy_sink_mask(core->policy, mask);
        return 0;
}


/// @brief Write log message to sinks based on mask.
/// @param mask 
/// @param buf 
/// @param len 
static void sink_write_masked(log_core_t *core,uint32_t mask, const char *buf, size_t len) 
{       
        pthread_mutex_lock(&core->mt_sink);
        log_sink_t *sf = (mask & LOG_SINK_FILE) ? log_sink_ref(core->sink_file) : NULL;
        log_sink_t *sn = (mask & LOG_SINK_UDP) ? log_sink_ref(core->sink_net) : NULL;
        log_sink_t *sc = (mask & LOG_SINK_UI) ? log_sink_ref(core->sink_ui) : NULL;
        pthread_mutex_unlock(&core->mt_sink);
        if (sf && sf->ops && sf->ops->write) {
                if (sf->ops->write(sf->ctx, buf, len) != 0) {
                        // __sync_add_and_fetch(&g_log_core.stats.sinks_write_err, 1);
                }
        }
        if (sn && sn->ops && sn->ops->write) {
                if (sn->ops->write(sn->ctx, buf, len) != 0) {
                        // __sync_add_and_fetch(&g_log_core.stats.sinks_write_err, 1);
                }
        }
        if (sc && sc->ops && sc->ops->write) {
                if (sc->ops->write(sc->ctx, buf, len) != 0) {
                        // __sync_add_and_fetch(&g_log_core.stats.sinks_write_err, 1);
                }
        }
        log_sink_unref(sf);
        log_sink_unref(sn);
        log_sink_unref(sc);
}      

// rotate if needed; enqueue rotate event for rotate worker
/// @brief 
/// @param p 
/// @param sinks_mask 
static void sink_file_rotate(log_core_t *core, log_policy_t *p, unsigned sinks_mask) 
{
        if (!(sinks_mask & LOG_SINK_FILE)) {
                return;
        }
        
        pthread_mutex_lock(&core->mt_sink);
        log_sink_t *sf = log_sink_ref(core->sink_file);
        pthread_mutex_unlock(&core->mt_sink);
        if (!sf) {
                return;
        }

        size_t sz = 0;
        if (!sf->ops || !sf->ops->get_size || sf->ops->get_size(sf->ctx, &sz)!=0) {
                log_sink_unref(sf);
                return;
        }
        int need = 0;
        if (p) {
                need = need_rotate_log_policy(p, sz);
        }
        if (!need) { 
                log_sink_unref(sf); 
                return; 
        }

        char rotated[256] = {0};
        int rc = -1;
        if (sf->ops && sf->ops->rotate) {
                bool remain_first_done = false; 
                rc = sf->ops->rotate(sf->ctx, need_first_remain_log_policy(p, sf->ops->get_first_remain_done(sf->ctx, &remain_first_done)), rotated, sizeof(rotated));
        }

        if (rc == 0) {
                // arm_v7_atomic_add_and_fetch_u64(&g_log_core.stats.rotate_ok, 1); //__sync_add_and_fetch(&g_log_core.stats.rotate_ok, 1);
        } else if (rc == 1) {
                // arm_v7_atomic_add_and_fetch_u64(&g_log_core.stats.rotate_partial, 1); //__sync_add_and_fetch(&g_log_core.stats.rotate_partial, 1);
        } else {
                // arm_v7_atomic_add_and_fetch_u64(&g_log_core.stats.rotate_failed, 1); //__sync_add_and_fetch(&g_log_core.stats.rotate_failed, 1);
        }

        // rotate_alarm_update(rc==1, rc<0);

        // /* enqueue rotate event for exec_safe */
        // pthread_mutex_lock(&g_log_core.mt_rotate);
        // if (rotate_queue_push(&g_log_core.rotate, rotated, (rc==1), (rc<0)) != 0) {
        // //         /* if rotate-event queue is full, at least notify */
        // //         emit_oob("ROTATE", "ERR", "rotate event queue full");
        // } else {
        //         pthread_cond_signal(&g_log_core.cv_rotate);
        // }
        // pthread_mutex_unlock(&g_log_core.mt_rotate);

        log_sink_unref(sf);
}

/// @brief Log worker thread function.
/// @param arg Unused argument.
/// @return NULL.
static void* log_worker(void *arg)
{
        log_core_t *core = (log_core_t*)arg;
        if (!core) {
                return NULL;
        }
        while (!core->stop_log) {
                pthread_mutex_lock(&core->mt_log);
                while(!core->stop_log && core->log.sz == 0) {
                        pthread_cond_wait(&core->cv_log, &core->mt_log);
                }
                if (core->stop_log) {
                        break;
                }
                log_msg_t *msg = log_queue_pop(&core->log);
                // __sync_add_and_fetch(&g_log_core.stats.dequeued, 1);
                // size_t depth = g_log_core.log.sz;
                // size_t cap = g_log_core.log.cap;
                pthread_mutex_unlock(&core->mt_log);
                if (!msg) {
                        continue;
                }
                //===== 
                uint32_t sinks_mask = get_log_policy_sink_mask(core->policy);
                // printf("log_worker1: sinks_mask=%d msg=%s", sinks_mask, msg->text);
                sink_write_masked(core, sinks_mask, msg->text, strlen(msg->text));
                free(msg);
                // maybe_near_full(depth, cap);
                sink_file_rotate(core, core->policy, sinks_mask); // rotate if needed. using policy and file sink only.
                // maybe_near_full(depth, cap);
        }
        while (core->log.sz > 0) {
                pthread_mutex_lock(&core->mt_log);
                log_msg_t *msg = log_queue_pop(&core->log);
                pthread_mutex_unlock(&core->mt_log);
                if (!msg) {
                        continue;
                }
                //===== 
                uint32_t sinks_mask = get_log_policy_sink_mask(core->policy);
                // printf("log_worker2: sinks_mask=%d msg=%s", sinks_mask, msg->text);
                sink_write_masked(core, sinks_mask, msg->text, strlen(msg->text));
                free(msg);
                //===== 
                sink_file_rotate(core, core->policy, sinks_mask); // rotate if needed. using policy and file sink only.
                // maybe_near_full(depth, cap);
        }
        return NULL;
}

/// @brief Allocate and initialize a new log_core_t instance.
/// @return Pointer to the allocated log_core_t instance, or NULL on failure.
log_core_t* alloc_log_core(void)
{
        log_core_t *core = (log_core_t*)calloc(1, sizeof(log_core_t));
        if (!core) {
                return NULL;
        }

        pthread_mutex_init(&core->mt_log, NULL);
        pthread_cond_init(&core->cv_log, NULL);

        pthread_mutex_init(&core->mt_sink, NULL);

        core->inited = false;
        core->stop_log = false;
        core->ui_fd = -1;
        memset(&core->ui_sender, 0, sizeof(core->ui_sender));
        core->ui_sender_valid = 0;

        memset(&core->cfg, 0, sizeof(core->cfg));
        core->cfg.size = sizeof(core->cfg);
        core->cfg.version = 1;
        core->cfg.level = LOG_LVL_DBG;
        core->cfg.queue_size = DEF_LOG_QUEUE_SIZE;
        core->cfg.max_msg = DEF_LOG_MAX_MSG;

        core->policy = alloc_log_policy();
        if (!core->policy) {
                pthread_mutex_destroy(&core->mt_log);
                pthread_cond_destroy(&core->cv_log);
                pthread_mutex_destroy(&core->mt_sink);
                free(core);
                return NULL;
        }
        set_log_policy_min_level(core->policy, core->cfg.level);
        set_log_policy_file_max_files(core->policy, DEFAULT_LOG_FILE_MAX_COUNT);
        set_log_policy_file_rotate_size(core->policy, DEFAULT_LOG_FILE_MAX_SIZE);
        set_log_policy_sink_mask(core->policy, 0);
        set_log_policy_file_remain_first(core->policy, DEFAULT_LOG_FILE_REMAIN_FIRST);

        log_queue_init(&core->log, core->cfg.queue_size);

        pthread_attr_t pth_attr;
        pthread_attr_init(&pth_attr);
        pthread_attr_setscope(&pth_attr, PTHREAD_SCOPE_SYSTEM);
        pthread_attr_setstacksize(&pth_attr, 64 * 1024); // stack_size를 적절히 설정
        
        if (pthread_create(&core->pth_log, &pth_attr, log_worker, core) != 0) {
                log_sink_unref(core->sink_file);
                log_sink_unref(core->sink_net);
                log_sink_unref(core->sink_ui);
                if (core->policy) {
                        destroy_log_policy(&core->policy);
                        core->policy = NULL;
                }
                
                pthread_mutex_destroy(&core->mt_log);
                pthread_cond_destroy(&core->cv_log);
                pthread_mutex_destroy(&core->mt_sink);
                free(core);
                return NULL;
        }
        core->inited = true;

        return core;
}

/// @brief Destroy a log_core_t instance and free its resources.
/// @param core Pointer to the log_core_t instance to be destroyed.
void destroy_log_core(log_core_t **core)
{
        if (!core || !*core) {
                return;
        }
        log_core_t *c = *core;
        free(c);
        *core = NULL;
}

/// @brief Apply log core configuration.
/// @param core Pointer to log core.
/// @param in Pointer to input configuration.
/// @return 0 on success, -1 on failure.
int apply_log_core_cfg(log_core_t *core, const log_cfg_in_t *in)
{
        if (!core || !in) {
                printf("Invalid core or input configuration.\n");
                return -1;
        }
        if (in->size != sizeof(log_cfg_in_t) || in->version != LOG_CFG_DTO_VERSION) {
                printf("Input configuration size or version mismatch.\n");
                return -1;
        }
        pthread_mutex_lock(&core->mt_log);

        core->cfg.level = in->level;
        core->cfg.max_msg = in->max_msg ? in->max_msg : DEF_LOG_MAX_MSG;
        if (in->queue_cap != core->cfg.queue_size) {
                core->cfg.queue_size = in->queue_cap;
                core->log.cap = core->cfg.queue_size; // adjust queue capacity
        }
        set_log_policy_min_level(core->policy, core->cfg.level);

        pthread_mutex_unlock(&core->mt_log);

        int rc = -1;
        rc = apply_file_sink(core, in);
        if (rc != 0) {
                printf("apply_file_sink failed %d\n", rc);
                return rc;
        }
        rc = apply_udp_sink(core, in);
        if (rc != 0) {
                printf("apply_udp_sink failed %d\n", rc);
                return rc;
        }
        rc = apply_ui_sink(core, in);
        if (rc != 0) {
                printf("apply_ui_sink failed %d\n", rc);
                return rc;
        }
        return 0;
}

/// @brief Get log core configuration.
/// @param core Pointer to log core.
/// @param out Pointer to output configuration.
/// @return 0 on success, -1 on failure.
int get_log_core_cfg(log_core_t *core, log_cfg_out_t *out)
{
        if (!core || !out) {
                return -1;
        }
        if (!core->inited) {
                return -1;
        }
        pthread_mutex_lock(&core->mt_log);
        out->size = sizeof(log_cfg_out_t);
        out->version = LOG_CFG_DTO_VERSION;
        out->level = core->cfg.level;
        out->max_msg = core->cfg.max_msg;
        out->queue_cap = core->cfg.queue_size;
        out->file_enable = core->cfg.file.enable;
        snprintf(out->file_fpath, sizeof(out->file_fpath), "%s", core->cfg.file.fpath);
        out->max_file_size = core->cfg.file.max_file_size;
        out->max_file_count = core->cfg.file.max_file_count;

        out->udp_enable = core->cfg.udp.enable;
        snprintf(out->udp_ip, sizeof(out->udp_ip), "%s", core->cfg.udp.udp_ip);
        out->udp_port = core->cfg.udp.udp_port;

        out->ui_enable = core->cfg.ui.enable;
        pthread_mutex_unlock(&core->mt_log);
        return 0;
}

/// @brief Get log core status.
/// @param core Pointer to log core.
/// @param out Pointer to output status.
/// @return 0 on success, -1 on failure.
int get_log_core_status(log_core_t *core, log_status_out_t *out)
{
        if (!core || !out) {
                return -1;
        }
        if (!core->inited) {
                return -1;
        }
        pthread_mutex_lock(&core->mt_log);
        out->size = sizeof(log_status_out_t);
        out->version = LOG_CFG_DTO_VERSION;
        out->queue_depth = core->log.sz;
        //@todo: get sink status
        // out->file_cur_index = sink_file_get_cur_index(core->sink_file);
        // out->file_cur_size = sink_file_get_cur_size(core->sink_file);
        // out->file_opened = sink_file_is_opened(core->sink_file);
        // out->udp_connected = sink_udp_is_connected(core->sink_net);
        // out->ui_connected = sink_ui_is_connected(core->sink_ui);
        pthread_mutex_unlock(&core->mt_log);
        return 0;
}

/// @brief Set log core UI sender callback.
/// @param core Pointer to log core.
/// @param sender Pointer to log UI sender callback structure.
/// @return 0 on success, -1 on failure.
int set_log_core_ui_sender(log_core_t *core, const log_ui_sender_t *sender)
{
        log_sink_t *s;

        if (!core || !sender || !sender->send_fn) {
                return -1;
        }

        pthread_mutex_lock(&core->mt_log);
        core->ui_sender = *sender;
        core->ui_sender_valid = 1;
        pthread_mutex_unlock(&core->mt_log);

        s = get_log_sink(core, LOG_SINK_UI);
        if (s) {
                (void)log_sink_ui_set_sender(s, sender);
                log_sink_unref(s);
        }

        return 0;
}

/// @brief Attach a file descriptor to the log core for logging.
/// @param core Pointer to log core.
/// @param fd File descriptor to attach.
/// @return 0 on success, -1 on failure.
int attach_log_core_fd(log_core_t *core, int fd, uint64_t last_seen_wseq)
{
        log_ui_sender_t sender;
        int has_sender = 0;
        if (!core) {
                return -1;
        }
        if (fd < 0) {
                return -1;
        }
        if (!core->inited) {
                return -1;
        }
        pthread_mutex_lock(&core->mt_log);
        core->ui_fd = fd;
        if (core->ui_sender_valid) {
                sender = core->ui_sender;
                has_sender = 1;
        }
        pthread_mutex_unlock(&core->mt_log);

        log_sink_t *s = get_log_sink(core, LOG_SINK_UI);
        if (s) {
                if (has_sender) {
                        (void)log_sink_ui_set_sender(s, &sender);
                }
                (void)log_sink_ui_attach_fd(s, fd, last_seen_wseq);
                log_sink_unref(s);
        }
        
        return 0;
}

/// @brief Detach a file descriptor from the log core.
/// @param core Pointer to log core.
/// @param fd File descriptor to detach.
/// @return 0 on success, -1 on failure.
int detach_log_core_fd(log_core_t *core, int fd)
{
        (void)fd;
        if (!core) {
                return -1;
        }
        if (!core->inited) {
                return -1;
        }
        pthread_mutex_lock(&core->mt_log);
        core->ui_fd = -1;
        pthread_mutex_unlock(&core->mt_log);
        // if (!get_log_sink(core, LOG_SINK_UI)) {
        //         log_sink_ui_detach_fd(core->sink_ui, -1);
        // }
        log_sink_t *s = get_log_sink(core, LOG_SINK_UI);
        if (s) {
                (void)log_sink_ui_detach_fd(s, fd);
                log_sink_unref(s);
        }
        return 0;
}

/// @brief Format log line.
/// @param out Output buffer.
/// @param out_sz Output buffer size.
/// @param ts_ms Timestamp in milliseconds.
/// @param lvl Log level.
/// @param tag Log tag.
/// @param file Source file name.
/// @param line Source line number.
/// @param func Source function name.
/// @param msg Log message.
static void format_line(char *out, size_t out_sz, uint64_t ts_ms, log_level_t lvl, const char *tag, 
                        const char *file, const uint32_t line, const char* func,  const char *msg) 
{
#ifdef PRT_LOG_TIME_FORMAT_DATE
        time_t sec = (time_t)(ts_ms / 1000);
        uint32_t msec = (uint32_t)(ts_ms % 1000);
        struct tm tm_info;
        gmtime_r(&sec, &tm_info);
        char time_str[20];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info);
#ifdef DEBUG
        snprintf(out, out_sz, "[%s.%03u] [%s] [%s] %s:%u %s() %s\n", 
            time_str, msec, log_level_str(lvl), tag ? tag : "TAG", 
            file ? file : "", line, func ? func : "", msg ? msg : "");
#else
        (void)file; (void)line; (void)func;
        snprintf(out, out_sz, "[%s.%03u] [%s] [%s] %s\n", 
            time_str, msec, log_level_str(lvl), tag ? tag : "TAG", msg ? msg : "");
#endif
#else
#ifdef DEBUG
        snprintf(out, out_sz, "[%llu] [%s] [%s] %s:%u %s() %s\n", 
                (unsigned long long)ts_ms, log_level_str(lvl), tag ? tag : "TAG", 
                file? file:"", line, func ? func : "", msg ? msg : "");
#else
        (void)file; (void)line; (void)func;
        snprintf(out, out_sz, "[%llu] [%s] [%s] %s\n", 
                (unsigned long long)ts_ms, log_level_str(lvl), tag ? tag : "TAG", msg ? msg : "");
#endif
#endif
}

/// @brief Write log message with va_list.
/// @param lvl Log level.
/// @param tag Log tag.
/// @param file Source file name.
/// @param line Source line number.
/// @param func Source function name.
/// @param fmt Log message format.
/// @param ap va_list of additional arguments for log message format.
void write_log_v(log_core_t *core, log_level_t lvl, const char *tag, const char *file, int line, const char *func, const char *fmt, va_list ap)
{
        (void)file; (void)line; (void)func;

        if (!core) {
                return;
        }

        if (!core->inited) {
                return;
        }

        char msg[DEF_LOG_MAX_MSG];
        vsnprintf(msg, sizeof(msg), fmt, ap);

        log_record_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.ts_ms = now_ms();
        rec.level = lvl;
        rec.tag = tag ? tag : APP_EXEC_NAME;
        rec.file = file;
        rec.line = line;
        rec.func = func;
        rec.msg = msg;

        if (!filter_log_policy(core->policy, rec)) {
                //__sync_add_and_fetch(&core->stats.filtered, 1);
                return;
        }

        size_t line_max = core->cfg.max_msg ? core->cfg.max_msg : DEF_LOG_MAX_MSG;
        size_t alloc_sz = sizeof(log_record_t) + line_max;

        log_msg_t *m = (log_msg_t*)malloc(alloc_sz);
        if (!m) {
                //__sync_add_and_fetch(&core->stats.alloc_failed, 1);
                return;
        }

        m->ts_ms = rec.ts_ms;
        m->lvl = rec.level;
        snprintf(m->tag, sizeof(m->tag), "%s", rec.tag);

        format_line(m->text,
                    alloc_sz - sizeof(log_msg_t),
                    rec.ts_ms, lvl, rec.tag, rec.file, rec.line, rec.func, msg);

        pthread_mutex_lock(&core->mt_log);
        if (log_queue_push(&core->log, m) != 0) {
                //__sync_add_and_fetch(&core->stats.queue_full, 1);
                pthread_mutex_unlock(&core->mt_log);
                free(m);
                return;
        }
        pthread_cond_signal(&core->cv_log);
        pthread_mutex_unlock(&core->mt_log);
}