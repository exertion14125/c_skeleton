#include <stdio.h>
#include <errno.h>

#include "util/ini.h"

#include "util/log/internal/log_core.h"

#include "util/log/log_level.h"
#include "util/log/log_cfg_dto.h"
#include "util/log/log_cfg_adapter.h"
// #include "util/log/internal/log_cfg_ini.h" /// @todo Uncomment when ini loader is implemented.

static int log_cfg_load_from_ini_path(const char *path, log_cfg_in_t *cfg_in)
{
        if (!path || !cfg_in) {
                return -EINVAL;
        }
        ini_store_t store = {0};
        int rc = ini_load_to_memory(path, &store);
        if (rc < 0) {
                printf("Failed to load INI file to memory: %s, rc=%d\n", path, rc);
                return rc;
        }
        cfg_in->size = sizeof(log_cfg_in_t);
        cfg_in->version = LOG_CFG_DTO_VERSION;
        // Parse general configuration.
        cfg_in->level = (log_level_t)ini_mem_get_int_value(&store, "LOG", "min_level", LOG_LVL_INF);
        cfg_in->queue_cap = (size_t)ini_mem_get_int_value(&store, "LOG", "queue_cap", DEF_LOG_QUEUE_SIZE);
        cfg_in->max_msg = (size_t)ini_mem_get_int_value(&store, "LOG", "max_msg", DEF_LOG_MAX_MSG);
        // Parse file sink configuration.
        ini_mem_get_string_value(&store, "FILE", "log_file_path", cfg_in->file_fpath, sizeof(cfg_in->file_fpath), DEFAULT_LOG_FILE_FPATH);
        cfg_in->remain_first = ini_mem_get_bool_value(&store, "FILE", "remain_first", DEFAULT_LOG_FILE_REMAIN_FIRST);
        cfg_in->max_file_size = (size_t)ini_mem_get_int_value(&store, "FILE", "max_size", DEFAULT_LOG_FILE_MAX_SIZE);
        cfg_in->max_file_count = (size_t)ini_mem_get_int_value(&store, "FILE", "max_count", DEFAULT_LOG_FILE_MAX_COUNT);
        cfg_in->flush_line = (uint32_t)ini_mem_get_int_value(&store, "FILE", "flush_line", DEFAULT_LOG_FLUSH_LINE);
        cfg_in->fsync_line = (uint32_t)ini_mem_get_int_value(&store, "FILE", "fsync_line", DEFAULT_LOG_FSYNC_LINE);
        // Parse UDP sink configuration.
        ini_mem_get_string_value(&store, "UDP", "ip", cfg_in->udp_ip, sizeof(cfg_in->udp_ip), "");
        cfg_in->udp_port = (uint16_t)ini_mem_get_int_value(&store, "UDP", "port", 514);
        // Parse UI sink configuration.
        cfg_in->ui_enable = ini_mem_get_bool_value(&store, "UI", "log_ui_enable", false);
        cfg_in->uds_notify = ini_mem_get_bool_value(&store, "UI", "uds_notify", false);
        cfg_in->enable_no_ui = ini_mem_get_bool_value(&store, "UI", "enable_no_ui", false);
        return 0;
}

/// @brief Load log configuration from INI file path.
/// @param path INI file path.
/// @param cfg_in Pointer to output configuration.
/// @return 0 on success, negative errno on failure.
int log_cfg_load(const log_cfg_src_t *src, log_cfg_in_t *cfg_in)
{
        switch (src->kind) {
        case LOG_CFG_SRC_INI_PATH:
                return log_cfg_load_from_ini_path(src->u.path, cfg_in);
        default:
                return -ENOSYS;
        }
}