#ifndef __ENGINE_CFG_CFG_REPO_H__
#define __ENGINE_CFG_CFG_REPO_H__

#include <stdint.h>

typedef struct cfg_repo_s {
        int is_open;
        uint32_t revision;
        char path[128];
        int32_t last_adjust;
        int32_t last_modify;
} cfg_repo_t;

typedef struct cfg_adjust_req_s {
        int32_t value;
} cfg_adjust_req_t;

typedef struct cfg_modify_req_s {
        int32_t key;
        int32_t value;
} cfg_modify_req_t;

extern int cfg_repo_open(cfg_repo_t *r, const char *path);
extern int cfg_repo_adjust(cfg_repo_t *r, const cfg_adjust_req_t *req);
extern int cfg_repo_reopen(cfg_repo_t *r);
extern int cfg_repo_modify(cfg_repo_t *r, const cfg_modify_req_t *req);
extern int cfg_repo_close(cfg_repo_t *r);

#endif /* __ENGINE_CFG_CFG_REPO_H__ */