#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>

#include "util/log/log_system.h"
#include "util/log/internal/log_core.h"

static log_core_t *g_log_core = NULL;
static pthread_mutex_t g_log_sys_mt = PTHREAD_MUTEX_INITIALIZER;
static bool g_log_sys_inited = false;
static log_ui_sender_t g_log_ui_sender;
static int g_log_ui_sender_valid = 0;

/// @brief Get the global log core instance (unsafe, no locking).
/// @return Pointer to the global log core instance. 
static log_core_t* sys_core_get_unsafe(void)
{
        return g_log_core;
}

/// @brief Apply the UI sender to the log core without locking. Caller must hold g_log_sys_mt.
/// @param core Pointer to log core.
/// @return 0 on success, negative errno on failure.
static int apply_ui_sender_to_core_unsafe(log_core_t *core)
{
        if (!core) {
                return -1;
        }

        if (!g_log_ui_sender_valid || !g_log_ui_sender.send_fn) {
                return 0;
        }

        return set_log_core_ui_sender(core, &g_log_ui_sender);
}

/// @brief Set the UI sender for the log system. This allows the log core to send log lines to the UI subscriber through the provided sender callback.
/// @param sender Pointer to the UI sender structure containing the callback function and user data.
/// @return 0 on success, negative errno on failure.
int set_log_system_ui_sender(const log_ui_sender_t *sender)
{
        int rc = 0;

        if (!sender || !sender->send_fn) {
                return -1;
        }

        pthread_mutex_lock(&g_log_sys_mt);
        g_log_ui_sender = *sender;
        g_log_ui_sender_valid = 1;
        if (g_log_sys_inited && g_log_core) {
                rc = apply_ui_sender_to_core_unsafe(g_log_core);
        }
        pthread_mutex_unlock(&g_log_sys_mt);
        return rc;
}


/// @brief Initialize the log system.
/// @return 0 on success, negative errno on failure.
int  init_log_system(void)
{
        pthread_mutex_lock(&g_log_sys_mt);
        if (g_log_sys_inited) {
                pthread_mutex_unlock(&g_log_sys_mt);
                return 0;
        }
        g_log_core = alloc_log_core();
        if (!g_log_core) {
                pthread_mutex_unlock(&g_log_sys_mt);
                printf("Failed to allocate log core.\n");
                return -1;
        }
        //===== default configuration
        log_cfg_in_t default_cfg;
        memset(&default_cfg, 0, sizeof(default_cfg));
        default_cfg.size = sizeof(default_cfg);
        default_cfg.version = 1;
        default_cfg.level = LOG_LVL_DBG;
        default_cfg.queue_cap = DEF_LOG_QUEUE_SIZE;
        default_cfg.max_msg = DEF_LOG_MAX_MSG;
        char log_full_path[DEF_LOG_FPATH_LEN]="";
        
        snprintf(log_full_path, sizeof(log_full_path)-1, DEFAULT_LOG_FILE_FPATH);
        strncpy(default_cfg.file_fpath, log_full_path, sizeof(default_cfg.file_fpath) - 1);
        default_cfg.file_fpath[sizeof(default_cfg.file_fpath) - 1] = '\0';

        default_cfg.remain_first = DEFAULT_LOG_FILE_REMAIN_FIRST;
        default_cfg.max_file_size = DEFAULT_LOG_FILE_MAX_SIZE;
        default_cfg.max_file_count = DEFAULT_LOG_FILE_MAX_COUNT;
        default_cfg.flush_line = DEFAULT_LOG_FLUSH_LINE;
        default_cfg.fsync_line = DEFAULT_LOG_FSYNC_LINE;
        strncpy(default_cfg.udp_ip, "", sizeof(default_cfg.udp_ip)-1);
        default_cfg.udp_port = 514;
        default_cfg.ui_enable = false;
        // strncpy(default_cfg.uds_path, "", sizeof(default_cfg.uds_path)-1);
        // default_cfg.backlog_store = false;
        int ret = apply_log_core_cfg(g_log_core, &default_cfg);
        if (ret != 0) {
                destroy_log_core(&g_log_core);
                pthread_mutex_unlock(&g_log_sys_mt);
                printf("Failed to apply default log core configuration: %d\n", ret);
                return ret;
        }
        (void)apply_ui_sender_to_core_unsafe(g_log_core);
        g_log_sys_inited = true;
        pthread_mutex_unlock(&g_log_sys_mt);
        return 0;
}

/// @brief Exit the log system.
/// @param void
void exit_log_system(void)
{
        pthread_mutex_lock(&g_log_sys_mt);
        if (!g_log_sys_inited) {
                pthread_mutex_unlock(&g_log_sys_mt);
                return;
        }
        if (g_log_core) {
                destroy_log_core(&g_log_core);
        }
        g_log_sys_inited = false;
        memset(&g_log_ui_sender, 0, sizeof(g_log_ui_sender));
        g_log_ui_sender_valid = 0;
        pthread_mutex_unlock(&g_log_sys_mt);
}

/// @brief Apply log system configuration.
/// @param cfg_in Pointer to input configuration.
/// @return 0 on success, negative errno on failure.
int apply_log_system_cfg(const log_cfg_in_t *cfg_in)
{
        if (!cfg_in) {
                return -1;
        }
        pthread_mutex_lock(&g_log_sys_mt);
        log_core_t *core = sys_core_get_unsafe();
        pthread_mutex_unlock(&g_log_sys_mt);
        if (!core) {
                return -1;
        }
        return apply_log_core_cfg(core, cfg_in);
}

/// @brief Load log system configuration from source and apply it.
/// @param src Pointer to configuration source.
/// @return 0 on success, negative errno on failure.
int log_system_reload_from_source(const log_cfg_src_t *src)
{
        log_cfg_in_t in;
        int rc = log_cfg_load(src, &in);
        if (rc < 0) {
                return rc;
        }
        return apply_log_system_cfg(&in);
}

/// @brief Get log system configuration.
/// @param cfg_out Pointer to output configuration.
/// @return 0 on success, negative errno on failure.
int get_log_system_cfg(log_cfg_out_t *cfg_out)
{
        if (!cfg_out) {
                return -1;
        }
        pthread_mutex_lock(&g_log_sys_mt);
        log_core_t *core = sys_core_get_unsafe();
        pthread_mutex_unlock(&g_log_sys_mt);
        if (!core) {
                return -1;
        }
        return get_log_core_cfg(core, cfg_out);
}

/// @brief Get log system status.
/// @param status_out Pointer to output status.
/// @return 0 on success, negative errno on failure.
int get_log_system_status(log_status_out_t *status_out)
{
        if (!status_out) {
                return -1;
        }
        pthread_mutex_lock(&g_log_sys_mt);
        log_core_t *core = sys_core_get_unsafe();
        pthread_mutex_unlock(&g_log_sys_mt);
        if (!core) {
                return -1;
        }
        return get_log_core_status(core, status_out);
}

/// @brief Attach a UI file descriptor to the log system.
/// @param fd File descriptor for the UI.
/// @return 0 on success, negative errno on failure.
int attach_log_system_ui(int fd, uint64_t last_seen_wseq)
{
        if (fd < 0) {
                return -1;
        }
        pthread_mutex_lock(&g_log_sys_mt);
        int inited = g_log_sys_inited;
        log_core_t *core = sys_core_get_unsafe();
        pthread_mutex_unlock(&g_log_sys_mt);
        if (!core || !inited) {
                return -1;
        }
        return attach_log_core_fd(core, fd, last_seen_wseq);
}

/// @brief Detach a UI file descriptor from the log system.
/// @param fd File descriptor for the UI.
/// @return 0 on success, negative errno on failure.
int detach_log_system_ui(int fd)
{
        if (fd < 0) {
                return -1;
        }
        pthread_mutex_lock(&g_log_sys_mt);
        int inited = g_log_sys_inited;
        log_core_t *core = sys_core_get_unsafe();
        pthread_mutex_unlock(&g_log_sys_mt);
        if (!core || !inited) {
                return -1;
        }
        return detach_log_core_fd(core, fd);
}

/// @brief Write a log message.
/// @param lvl Log level.
/// @param tag Log tag.
/// @param file Source file name.
/// @param line Source line number.
/// @param func Source function name.
/// @param fmt Log message format.
/// @param ... Additional arguments for log message format.
void write_log(log_level_t lvl, const char *tag, const char *file, int line, const char *func, const char *fmt, ...)
{
        if (!fmt) {
                return;
        }
        pthread_mutex_lock(&g_log_sys_mt);
        int inited = g_log_sys_inited;
        log_core_t *core = sys_core_get_unsafe();
        pthread_mutex_unlock(&g_log_sys_mt);
        if (!core || !inited) {
                return;
        }
        va_list ap;
        va_start(ap, fmt);
        write_log_v(core, lvl, tag, file, line, func, fmt, ap);
        va_end(ap);
}

/// @brief Write a log message with va_list.
/// @param lvl Log level.
/// @param tag Log tag.
/// @param file Source file name.
/// @param line Source line number.
/// @param func Source function name.
/// @param fmt Log message format.
/// @param ap va_list of additional arguments for log message format.
void write_log_vp(log_level_t lvl, const char *tag, const char *file, int line, const char *func, const char *fmt, va_list ap)
{
        pthread_mutex_lock(&g_log_sys_mt);
        int inited = g_log_sys_inited;
        log_core_t *core = sys_core_get_unsafe();
        pthread_mutex_unlock(&g_log_sys_mt);
        if (!core || !inited) {
                return;
        }
        write_log_v(core, lvl, tag, file, line, func, fmt, ap);
}