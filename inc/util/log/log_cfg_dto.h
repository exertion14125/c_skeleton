#ifndef __LOG_CFG_DTO_H__
#define __LOG_CFG_DTO_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "log_level.h"

#define LOG_CFG_DTO_VERSION    1

/// @brief Log configuration data transfer object structure. Using Request DTO.
struct log_cfg_in_s {
        uint32_t size; ///< Size of this structure. For check version.
        uint32_t version; ///< Version of this structure. For future compatibility.
        /// General configuration.
        log_level_t level; ///< Minimum log level.
        size_t queue_cap; ///< Log queue capacity.
        size_t max_msg; ///< Maximum log message size.
        /// File sink configuration.
        char file_fpath[256]; ///< Log file path.
        bool remain_first; ///< Remain first log file or not.
        size_t max_file_size; ///< Maximum size of each log file.
        size_t max_file_count; ///< Maximum number of log files for rotation.
        uint32_t flush_line; ///< Log flush line count. (0: never, 1: every line, N: every N lines)
        uint32_t fsync_line; ///< Log fsync line count. (0: never, 1: every line, N: every N lines)
        /// UDP sink configuration.
        char udp_ip[64]; ///< UDP target IP address.
        uint16_t udp_port; ///< UDP target port.
        /// UI sink configuration.
        bool ui_enable; ///< UI sink enable or not.
        // char uds_path[256]; ///< UI sink UDS path.
        bool uds_notify; ///< UI sink UDS notify enable or not.
        bool enable_no_ui; ///< UI sink enable when no UI.
        // bool backlog_store; ///< UI sink backlog store enable or not.
};
typedef struct log_cfg_in_s log_cfg_in_t;

/// @brief Log configuration data transfer object structure. Using Snapshot DTO.
struct log_cfg_out_s {
        uint32_t size; ///< Size of this structure. For check version.
        uint32_t version; ///< Version of this structure. For future compatibility.
        /// General configuration.
        log_level_t level; ///< Minimum log level.
        size_t queue_cap; ///< Log queue capacity.
        size_t max_msg; ///< Maximum log message size.
        /// File sink configuration.
        bool file_enable;
        char file_fpath[256]; ///< Log file path.
        size_t max_file_size; ///< Maximum size of each log file.
        size_t max_file_count; ///< Maximum number of log files for rotation.
        /// UDP sink configuration.
        bool udp_enable;
        char udp_ip[64]; ///< UDP target IP address.
        uint16_t udp_port; ///< UDP target port.
        /// UI sink configuration.
        bool ui_enable;
        bool ui_dedupe_on; ///< UI sink duplicates enable or not.
};
typedef struct log_cfg_out_s log_cfg_out_t;

struct log_status_out_s {
        uint32_t size; ///< Size of this structure. For check version.
        uint32_t version; ///< Version of this structure. For future compatibility.
        /// General status.
        size_t queue_depth; ///< Current log queue depth.
        size_t dropped_msgs; ///< Total dropped log messages due to full queue.
        /// File sink status.
        int file_opened; ///< File sink opened or not.
        size_t file_cur_size; ///< Current log file size.
        size_t file_cur_index; ///< Current log file index.
        /// UDP sink status.
        int udp_connected; ///< UDP sink connected or not.
        /// UI sink status.
        int ui_has_subscriber; ///< UI sink has subscriber or not.
};
typedef struct log_status_out_s log_status_out_t;

#endif /* __LOG_CFG_DTO_H__ */