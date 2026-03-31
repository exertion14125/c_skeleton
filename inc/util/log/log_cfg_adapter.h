#ifndef __LOG_CFG_ADPATER_H__
#define __LOG_CFG_ADPATER_H__

/// @todo Add more source kinds (JSON, SHM, etc).
#define LOG_CFG_SRC_INI_PATH (0)

struct log_cfg_src_s {
        int kind;
        union {
                const char *path; /// ini file path. 
        } u;
};
typedef struct log_cfg_src_s log_cfg_src_t;

extern int log_cfg_load(const log_cfg_src_t *src, log_cfg_in_t *cfg_in);

#endif /* __LOG_CFG_ADPATER_H__ */