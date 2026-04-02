#ifndef __UTIL_OBS_SINK_LOG_H__
#define __UTIL_OBS_SINK_LOG_H__

#include <stdint.h>
#include <stdarg.h>

#include "util/obs/obs.h"
#include "util/obs/obs_types.h"

typedef void (*obs_log_vwrite_fn)( void *user, int lvl, const char *tag, const char *file, int line, const char *func, const char *fmt, va_list ap);

typedef struct obs_sink_log_s obs_sink_log_t;

/// @brief Configuration structure for the obs_sink_log.
struct obs_sink_log_cfg_s {
        uint32_t size;             /// sizeof(obs_sink_log_cfg_t)
        uint32_t version;          /// reserved
        int      enable;           /// 0 disable, 1 enable
        int      guard_enable;     /// recursion guard
        char     tag[24];          /// default "obs"

        uint32_t type_allow_mask; /// type_allow_mask == 0: allow all, otherwise allow only types with (type_allow_mask & (1<<(type-1))) != 0

        /// logging hook injection (required)
        obs_log_vwrite_fn vwrite;
        void             *vwrite_user;

        int include_src; /// optional: include src info (file/line/func) in vwrite callback.
};
typedef struct obs_sink_log_cfg_s obs_sink_log_cfg_t;

///===== Lifecycle =====///
extern obs_sink_log_t *alloc_obs_sink_log(void);
extern void destroy_obs_sink_log(obs_sink_log_t **ps);

///===== Init =====///
extern int obs_sink_log_init(obs_sink_log_t *s, const obs_sink_log_cfg_t *cfg);

///===== Emit =====///
extern int obs_sink_log_emit(obs_sink_log_t *s, const obs_evt_t *e);

///===== Drain =====///
extern int obs_sink_log_drain(obs_sink_log_t *s, obs_t *o, uint32_t max_n);

// log write adpater. set bootstrap function
// static void obs_log_vwrite_adapter(void *user, int lvl,
//                                    const char *tag,
//                                    const char *file, int line, const char *func,
//                                    const char *fmt, va_list ap)
// {
//         (void)user;
//         write_log_vp((log_level_t)lvl, tag, file, line, func, fmt, ap);
// }
#endif /* __UTIL_OBS_SINK_LOG_H__ */
