#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include "util/log/internal/log_policy.h"

struct log_policy_s {
        /// General policy.
        log_level_t min_level;
        /// Sink policy.
        uint32_t sink_mask;
        /// File sink policy.
        bool remain_first; ///< Remain first log file or not
        uint32_t max_files; ///< Maximum number of log files for rotation
        size_t rotate_size_bytes; ///< Rotate log file size in bytes
};
log_policy_t* alloc_log_policy(void)
{
        log_policy_t *policy = (log_policy_t*)malloc(sizeof(log_policy_t));
        if(!policy) {
                return NULL;
        }
        memset(policy, 0, sizeof(log_policy_t));
        //====== Set default values.
        policy->min_level = LOG_LVL_DBG;
        policy->sink_mask = LOG_SINK_FILE;
        policy->remain_first = true;
        policy->max_files = 1;
        policy->rotate_size_bytes = 10 * 1024; // 10 KB
        return policy;
}

/// @brief Destroy log policy.
/// @param policy Pointer to log policy to be destroyed.
void destroy_log_policy(log_policy_t **policy)
{
        if(*policy) {
                free(*policy);
                *policy = NULL;
        }
}

/// @brief Set log policy minimum level.
/// @param policy Pointer to log policy.
/// @param level Minimum log level.
/// @return 0 on success, -1 on failure.
int  set_log_policy_min_level(log_policy_t *policy, log_level_t level)
{
        if(!policy) {
                return -1;
        }
        policy->min_level = level;
        return 0;
}

/// @brief Set log policy sink mask.
/// @param policy Pointer to log policy.
/// @param sink_mask Sink mask.
/// @return 0 on success, -1 on failure.
int  set_log_policy_sink_mask(log_policy_t *policy, uint32_t sink_mask)
{
        if(!policy) {
                return -1;
        }
        policy->sink_mask = sink_mask;
        return 0;
}

/// @brief Set log policy file remain first.
/// @param policy Pointer to log policy.
/// @param remain_first Remain first log file or not.
/// @return 0 on success, -1 on failure.
int  set_log_policy_file_remain_first(log_policy_t *policy, bool remain_first)
{
        if(!policy) {
                return -1;
        }
        policy->remain_first = remain_first;
        return 0;
}

/// @brief Set log policy file maximum files.
/// @param policy Pointer to log policy.
/// @param max_files Maximum number of log files for rotation.
/// @return 0 on success, -1 on failure.
int  set_log_policy_file_max_files(log_policy_t *policy, uint32_t max_files)
{
        if(!policy) {
                return -1;
        }
        policy->max_files = max_files;
        return 0;
}

/// @brief Set log policy file rotate size.
/// @param policy Pointer to log policy.
/// @param rotate_size_bytes Rotate log file size in bytes.
/// @return 0 on success, -1 on failure.
int  set_log_policy_file_rotate_size(log_policy_t *policy, size_t rotate_size_bytes)
{
        if(!policy) {
                return -1;
        }
        policy->rotate_size_bytes = rotate_size_bytes;
        return 0;
}

/// @brief Get log policy minimum level.
/// @param policy Pointer to log policy.
/// @return Minimum log level. If policy is NULL, returns LOG_LVL_ERR.
log_level_t get_log_policy_min_level(const log_policy_t *policy)
{
        if(!policy) {
                return LOG_LVL_ERR;
        }
        return policy->min_level;
}

/// @brief Get log policy sink mask.
/// @param policy Pointer to log policy.
/// @return Sink mask. If policy is NULL, returns 0.
uint32_t    get_log_policy_sink_mask(const log_policy_t *policy)
{
        if(!policy) {
                return 0;
        }
        return policy->sink_mask;
}

/// @brief Get log policy file remain first.
/// @param policy Pointer to log policy.
/// @return Whether the file remain first. If policy is NULL, returns true.
bool        get_log_policy_file_remain_first(const log_policy_t *policy)
{
        if(!policy) {
                return true;
        }
        return policy->remain_first;
}
/// @brief Get log policy file maximum files.
/// @param policy Pointer to log policy.
/// @return Maximum number of log files for rotation. If policy is NULL, returns 0
uint32_t    get_log_policy_file_max_files(const log_policy_t *policy)
{
        if(!policy) {
                return 0;
        }
        return policy->max_files;
}

/// @brief Get log policy file rotate size.
/// @param policy Pointer to log policy.
/// @return Rotate log file size in bytes. If policy is NULL, returns 0.
size_t      get_log_policy_file_rotate_size(const log_policy_t *policy)
{
        if(!policy) {
                return 0;
        }
        return policy->rotate_size_bytes;

}

/// @brief Filter log record based on policy.
/// @param policy Pointer to log policy.
/// @param rec Log record to be filtered.
/// @return 1 if the log record passes the filter, 0 otherwise.
int filter_log_policy(const log_policy_t *policy, log_record_t rec)
{
        if(!policy) {
                return 0;
        }
        return rec.level >= policy->min_level;
}

/// @brief Check if log file should be rotated based on policy and current size.
/// @param policy Pointer to log policy. 
/// @param curr_sz Current size of the log file in bytes.
/// @return 1 if the log file should be rotated, 0 otherwise.
int need_rotate_log_policy(const log_policy_t *policy, size_t curr_sz)
{
        if(!policy) {
                return 0;
        }
        return curr_sz >= policy->rotate_size_bytes;
}

/// @brief Check if first log file needs to be remained based on policy.
/// @param policy Pointer to log policy.
/// @param out_remain_first_done Pointer to boolean indicating if the first remain is already done.
/// @return 1 if the first log file needs to be remained, 0 otherwise.
bool need_first_remain_log_policy(const log_policy_t *policy, bool remain_first_done)
{
        if (!policy) {
                return false;
        }
        return policy->remain_first && !remain_first_done;
}
