/// @file       log_core.h
/// @brief      Core logging functionalities: initialization, writing logs, and managing policies and sinks.
/// @author     KIM JEONGGI (jgkim12@digitech.kr)

#ifndef __LOG_CORE_H__
#define __LOG_CORE_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include "util/log/log_level.h"
#include "util/log/log_cfg_dto.h"

#define PRT_LOG_TIME_FORMAT_DATE 1

typedef struct log_core_s log_core_t;

#define DEF_LOG_QUEUE_SIZE 1024 ///< Define log queue size
#define DEF_LOG_MAX_MSG 1024 ///< Define log max message size
#define DEF_LOG_FPATH_LEN 256 ///< Define log file path length
#define DEF_LOG_UDP_IP_LEN 64 ///< Define log UDP IP length
#define DEF_LOG_ROTATE_QUEUE_SIZE 64 ///< Define log rotate queue size

#define DEFAULT_LOG_FILE_FPATH "/tmp/"APP_EXEC_NAME"_log.log" ///< Default log file path
#define DEFAULT_LOG_FILE_MAX_SIZE  (10 * 1024)  ///< Default log file max size: 10 KB
#define DEFAULT_LOG_FILE_MAX_COUNT (1) ///< Default log file max count
#define DEFAULT_LOG_FILE_REMAIN_FIRST  (true)  ///< Default log file remain first file or not
#define DEFAULT_LOG_FLUSH_LINE  (1) ///< Default log flush line count (0: never, 1: every line, N: every N lines)
#define DEFAULT_LOG_FSYNC_LINE  (0) ///< Default log fsync line count (0: never, 1: every line, N: every N lines)

//===== lifecycle =====//
extern log_core_t* alloc_log_core(void);
extern void destroy_log_core(log_core_t **core);

//===== config apply and shapshot =====//
extern int apply_log_core_cfg(log_core_t *core, const log_cfg_in_t *in);
extern int get_log_core_cfg(log_core_t *core, log_cfg_out_t *out);
extern int get_log_core_status(log_core_t *core, log_status_out_t *out);

//====== ui attach/detach ======//
extern int attach_log_core_fd(log_core_t *core, int fd, uint64_t last_seen_wseq); // fd: UDS socket fd
extern int detach_log_core_fd(log_core_t *core, int fd); // fd: UDS socket fd

//===== helpers API =====//
extern void write_log_v(log_core_t* core, log_level_t lvl, const char *tag, const char *file, int line, const char *func, const char *fmt, va_list ap);

#endif /* __LOG_CORE_H__ */