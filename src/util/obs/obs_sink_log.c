#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "util/obs/obs_sink_log.h"

#ifndef OBS_SINK_LOG_VER
#define OBS_SINK_LOG_VER 0x00010000u
#endif

#ifndef OBS_SINK_LOG_DEFAULT_TAG
#define OBS_SINK_LOG_DEFAULT_TAG "obs"
#endif

static volatile int g_obs_sink_log_guard = 0; ///< recursion guard global variable

/// @brief Sink log structure
struct obs_sink_log_s {
        obs_sink_log_cfg_t cfg; ///< configuration
};

/// @brief Check if the observation event type is allowed by the configuration
/// @param c configuration pointer
/// @param type observation event type
/// @return 1 if allowed, 0 if not allowed
static int obs_type_allowed(const obs_sink_log_cfg_t *c, uint32_t type)
{
        if (!c) return 0;
        if (c->type_allow_mask == 0) return 1;
        if (type >= 1 && type <= 31) {
                uint32_t bit = (1u << (type - 1));
                return (c->type_allow_mask & bit) ? 1 : 0;
        }
        return 0;
}

/// @brief Map observation event type to string
/// @param type observation event type
/// @return string representation of the event type
static const char *obs_type_to_str(uint32_t type)
{
        switch (type) {
        case OBS_EVT_FSM:          return "FSM";
        case OBS_EVT_DISPATCH:     return "DISPATCH";
        case OBS_EVT_DROP:         return "DROP";
        case OBS_EVT_DROP_EPOCH:   return "DROP_EPOCH";
        case OBS_EVT_DROP_REQ:     return "DROP_REQ";
        case OBS_EVT_DROP_POLICY:  return "DROP_POLICY";
        case OBS_EVT_DROP_FULL:    return "DROP_FULL";
        case OBS_EVT_CANCEL_ALL:   return "CANCEL_ALL";
        case OBS_EVT_CANCEL_REQ:   return "CANCEL_REQ";
        case OBS_EVT_FSM_TR:       return "FSM_TRANSITION";
        case OBS_EVT_FSM_ENTER:    return "FSM_ENTER";
        case OBS_EVT_FSM_EXIT:     return "FSM_EXIT";
        case OBS_EVT_FSM_STEP:     return "FSM_STEP";
        case OBS_EVT_FSM_GUARD_FAIL: return "FSM_GUARD_FAIL";
        default:                   return "UNKNOWN";
        }
}

/// @brief Map observation event type to log level integer
/// @param type observation event type
/// @return log level integer
static int obs_type_to_loglevel_int(uint32_t type)
{
        switch (type) {
        case OBS_EVT_DROP:
        case OBS_EVT_DROP_EPOCH:
        case OBS_EVT_DROP_REQ:
        case OBS_EVT_DROP_POLICY:
        case OBS_EVT_DROP_FULL:
                return 2; // WARN
        case OBS_EVT_CANCEL_ALL:
        case OBS_EVT_CANCEL_REQ:
                return 1; //INFO
        default:
                return 0; //DBG
        }
}

/// @brief Call the vwrite function of the sink log
/// @param s sink log pointer
/// @param lvl log level
/// @param tag log tag
/// @param file source file
/// @param line source line
/// @param func source function
/// @param fmt format string
static void obs_sink_log_call_vwrite(obs_sink_log_t *s,
                                     int lvl, const char *tag,
                                     const char *file, int line, const char *func,
                                     const char *fmt, ...)
{
        va_list ap;
        if (!s || !s->cfg.vwrite) return;

        va_start(ap, fmt);
        s->cfg.vwrite(s->cfg.vwrite_user, lvl, tag, file, line, func, fmt, ap);
        va_end(ap);
}

/// @brief Allocate the sink log
/// @return pointer to sink log, NULL on error
obs_sink_log_t *alloc_obs_sink_log(void)
{
        return (obs_sink_log_t*)calloc(1, sizeof(obs_sink_log_t));
}

/// @brief Destroy the sink log
/// @param ps pointer to sink log pointer
void destroy_obs_sink_log(obs_sink_log_t **ps)
{
        if (ps && *ps) {
                free(*ps);
                *ps = NULL;
        }
}

/// @brief Initialize the sink log
/// @param s sink log pointer
/// @param cfg configuration pointer
/// @return 0 on success, <0 on error
int obs_sink_log_init(obs_sink_log_t *s, const obs_sink_log_cfg_t *cfg)
{
        obs_sink_log_cfg_t c;

        if (!s) return -1;

        memset(&c, 0, sizeof(c));
        c.size = (uint32_t)sizeof(c);
        c.version = OBS_SINK_LOG_VER;
        c.enable = 1;
        c.guard_enable = 1;
        (void)snprintf(c.tag, sizeof(c.tag), "%s", OBS_SINK_LOG_DEFAULT_TAG);
        c.type_allow_mask = 0;
        c.vwrite = NULL;
        c.vwrite_user = NULL;
        c.include_src = 0;

        if (cfg && cfg->size >= sizeof(obs_sink_log_cfg_t)) {
                c = *cfg;
                if (c.tag[0] == '\0') {
                        (void)snprintf(c.tag, sizeof(c.tag), "%s", OBS_SINK_LOG_DEFAULT_TAG);
                }
        }

        if (!c.vwrite) {
                return -2; /* injection required */
        }

        s->cfg = c;
        return 0;
}

/// @brief Emit an event to the sink log
/// @param s sink log pointer
/// @param e event pointer
/// @return 1 on success, 0 if not emitted, <0 on error
int obs_sink_log_emit(obs_sink_log_t *s, const obs_evt_t *e)
{
        const char *tag;
        int lvl;

        if (!s || !e) return -1;
        if (!s->cfg.enable) return 0;
        if (!obs_type_allowed(&s->cfg, e->type)) return 0;

        tag = (s->cfg.tag[0] ? s->cfg.tag : OBS_SINK_LOG_DEFAULT_TAG);
        lvl = obs_type_to_loglevel_int(e->type);

        if (s->cfg.guard_enable) {
                if (__sync_lock_test_and_set(&g_obs_sink_log_guard, 1)) return 0;
        }
        /// type payload format 
        switch (e->type) {
        /// a=before, b=ev, c=after
        case OBS_EVT_FSM: {
                obs_sink_log_call_vwrite(
                        s, lvl, tag, "", 0, "",
                        "obs ts=%llu type=%s(%u) before=%d ev=%d after=%d",
                        (unsigned long long)e->ts_ms,
                        obs_type_to_str(e->type), (unsigned)e->type,
                        (int)e->a, (int)e->b, (int)e->c);
        } break;
        case OBS_EVT_DISPATCH: {
                obs_sink_log_call_vwrite(
                        s, lvl, tag, "", 0, "",
                        "obs ts=%llu type=%s(%u) ev=%d req_id=%u rc=%d",
                        (unsigned long long)e->ts_ms,
                        obs_type_to_str(e->type), (unsigned)e->type,
                        (int)e->a, (unsigned)(uint32_t)e->b,
                        (int)e->c);
        } break;
        /// DROP_*: a=ev, b=req_id, c=0
        case OBS_EVT_DROP_EPOCH:
        case OBS_EVT_DROP_REQ:
        case OBS_EVT_DROP_POLICY: 
        case OBS_EVT_DROP_FULL: {
                obs_sink_log_call_vwrite(
                        s, lvl, tag, "", 0, "",
                        "obs ts=%llu type=%s(%u) ev=%d req_id=%u",
                        (unsigned long long)e->ts_ms,
                        obs_type_to_str(e->type), (unsigned)e->type,
                        (int)e->a, (unsigned)(uint32_t)e->b);
        } break;
        ///CANCEL_ALL: a=0, b=epoch_low32, c=epoch_high32
        case OBS_EVT_CANCEL_ALL: {
                uint64_t ep =
                        ((uint64_t)(uint32_t)e->b) |
                        (((uint64_t)(uint32_t)e->c) << 32);
                obs_sink_log_call_vwrite(
                        s, lvl, tag, "", 0, "",
                        "obs ts=%llu type=%s(%u) epoch=%llu",
                        (unsigned long long)e->ts_ms,
                        obs_type_to_str(e->type), (unsigned)e->type,
                        (unsigned long long)ep);
        } break;
        /// CANCEL_REQ: a=target_req_id, b=epoch_low32, c=epoch_high32
        case OBS_EVT_CANCEL_REQ: {
                uint64_t ep =
                        ((uint64_t)(uint32_t)e->b) |
                        (((uint64_t)(uint32_t)e->c) << 32);
                obs_sink_log_call_vwrite(
                        s, lvl, tag, "", 0, "",
                        "obs ts=%llu type=%s(%u) target_req_id=%u epoch=%llu",
                        (unsigned long long)e->ts_ms,
                        obs_type_to_str(e->type), (unsigned)e->type,
                        (unsigned)(uint32_t)e->a,
                        (unsigned long long)ep);
        } break;
        /// DEFER : a=ev, b=req_id, c=wait_ms(0=infinite)
        case OBS_EVT_DEFER: {
                obs_sink_log_call_vwrite(
                        s, lvl, tag, "", 0, "",
                        "obs ts=%llu type=%s(%u) ev=%d req_id=%u wait_ms=%u",
                        (unsigned long long)e->ts_ms,
                        obs_type_to_str(e->type), (unsigned)e->type,
                        (int)e->a, (unsigned)(uint32_t)e->b,
                        (unsigned)(uint32_t)e->c);
        } break;
        case OBS_EVT_FSM_TR: {
                obs_sink_log_call_vwrite(
                        s, lvl, tag, "", 0, "",
                        "obs ts=%llu type=%s(%u) before=%d ev=%d after=%d",
                        (unsigned long long)e->ts_ms,
                        obs_type_to_str(e->type), (unsigned)e->type,
                        (int)e->a, (int)e->b, (int)e->c);
        } break;
        case OBS_EVT_FSM_ENTER: {
                obs_sink_log_call_vwrite(
                        s, lvl, tag, "", 0, "",
                        "obs ts=%llu type=%s(%u) st=%d ev=%d",
                        (unsigned long long)e->ts_ms,
                        obs_type_to_str(e->type), (unsigned)e->type,
                        (int)e->a, (int)e->b);
        } break;
        case OBS_EVT_FSM_EXIT: {
                obs_sink_log_call_vwrite(
                        s, lvl, tag, "", 0, "",
                        "obs ts=%llu type=%s(%u) st=%d ev=%d",
                        (unsigned long long)e->ts_ms,
                        obs_type_to_str(e->type), (unsigned)e->type,
                        (int)e->a, (int)e->b);
        } break;
        case OBS_EVT_FSM_GUARD_FAIL: {
                obs_sink_log_call_vwrite(
                        s, lvl, tag, "", 0, "",
                        "obs ts=%llu type=%s(%u) st=%d ev=%d next=%d",
                        (unsigned long long)e->ts_ms,
                        obs_type_to_str(e->type), (unsigned)e->type,
                        (int)e->a, (int)e->b, (int)e->c);
        } break;
        case OBS_EVT_FSM_STEP: {
                int after = (int)((uint32_t)e->c >> 16);
                int rc16  = (int)((uint32_t)e->c & 0xFFFFu);
                obs_sink_log_call_vwrite(
                        s, lvl, tag, "", 0, "",
                        "obs ts=%llu type=%s(%u) before=%d ev=%d after=%d rc=%d",
                        (unsigned long long)e->ts_ms,
                        obs_type_to_str(e->type), (unsigned)e->type,
                        (int)e->a, (int)e->b, after, rc16);
        } break;
        /// fallback: raw dump
        default: {
                obs_sink_log_call_vwrite(
                        s, lvl, tag, "", 0, "",
                        "obs ts=%llu type=%s(%u) a=%d b=%d c=%d",
                        (unsigned long long)e->ts_ms,
                        obs_type_to_str(e->type), (unsigned)e->type,
                        (int)e->a, (int)e->b, (int)e->c);
        } break;
        }
        if (s->cfg.guard_enable) {
                __sync_lock_release(&g_obs_sink_log_guard);
        }
        return 1;
}

/// @brief Drain events from observation to sink log
/// @param s sink log pointer
/// @param o observation pointer
/// @param max_n maximum number of events to drain (0: unlimited)
/// @return number of drained events, <0 on error
int obs_sink_log_drain(obs_sink_log_t *s, obs_t *o, uint32_t max_n)
{
        uint32_t n = 0;
        int rc;
        obs_evt_t e;

        if (!s || !o) return -1;

        while (max_n == 0 || n < max_n) { //max_n==0 => unlimited
                rc = obs_pop(o, &e);
                if (rc < 0) return -2;
                if (rc == 0) break;

                (void)obs_sink_log_emit(s, &e);
                n++;
        }
        return (int)n;
}
