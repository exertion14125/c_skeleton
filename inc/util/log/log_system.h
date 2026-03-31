#ifndef __LOG_SYSTEM_H__
#define __LOG_SYSTEM_H__

#include <stdarg.h>

#include "log_level.h"
#include "log_cfg_dto.h"
#include "log_cfg_adapter.h"

//===== Lifecycle =====//
extern int  init_log_system(void);
extern void exit_log_system(void);

//===== Apply configuration =====//
extern int apply_log_system_cfg(const log_cfg_in_t *cfg_in);
extern int log_system_reload_from_source(const log_cfg_src_t *src);

//===== Get configuration =====//
extern int get_log_system_cfg(log_cfg_out_t *cfg_out);
extern int get_log_system_status(log_status_out_t *status_out);

//===== UI Sink helpers =====//
extern int attach_log_system_ui(int fd, uint64_t last_seen_wseq); // fd: UDS socket fd
extern int detach_log_system_ui(int fd); // fd: UDS socket fd

//===== Helpers API =====//
extern void write_log(log_level_t lvl, const char *tag, const char *file, int line, const char *func, const char *fmt, ...);
extern void write_log_vp(log_level_t lvl, const char *tag, const char *file, int line, const char *func, const char *fmt, va_list ap);
#endif /* __LOG_SYSTEM_H__ */