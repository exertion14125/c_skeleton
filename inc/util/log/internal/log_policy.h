#ifndef __LOG_POLICY_H__
#define __LOG_POLICY_H__

#include <stddef.h>
#include <stdint.h>

#include "util/log/log_level.h"
#include "util/log/internal/log_types.h"

#define LOG_SINK_FILE    (1u<<0) //< SINK FILE MASK BIT 0 .
#define LOG_SINK_UDP     (1u<<1) //< SINK NETWORK(UDP) MASK BIT 1 .
#define LOG_SINK_UI      (1u<<2) //< SINK UI MASK BIT 2 .

typedef struct log_record_s log_record_t;

typedef struct log_policy_s log_policy_t;

extern log_policy_t* alloc_log_policy(void);
extern void destroy_log_policy(log_policy_t **policy);

extern int  set_log_policy_min_level(log_policy_t *policy, log_level_t level);
extern int  set_log_policy_sink_mask(log_policy_t *policy, uint32_t sink_mask);
extern int  set_log_policy_file_remain_first(log_policy_t *policy, bool remain_first);
extern int  set_log_policy_file_max_files(log_policy_t *policy, uint32_t max_files);
extern int  set_log_policy_file_rotate_size(log_policy_t *policy, size_t rotate_size_bytes);

extern log_level_t get_log_policy_min_level(const log_policy_t *policy);
extern uint32_t    get_log_policy_sink_mask(const log_policy_t *policy);
extern bool        get_log_policy_file_remain_first(const log_policy_t *policy);
extern uint32_t    get_log_policy_file_max_files(const log_policy_t *policy);
extern size_t      get_log_policy_file_rotate_size(const log_policy_t *policy);

extern int  filter_log_policy(const log_policy_t *policy, log_record_t rec);
extern int  need_rotate_log_policy(const log_policy_t *policy, size_t curr_sz);
extern bool need_first_remain_log_policy(const log_policy_t *policy, bool remain_first_done);
#endif /* __LOG_POLICY_H__ */