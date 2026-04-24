#include <string.h>
#include <errno.h>

#include "engine/cfg/cfg_repo.h"

int cfg_repo_open(cfg_repo_t *r, const char *path)
{
        if (!r || !path || path[0] == '\0') {
                return -EINVAL;
        }

        memset(r, 0, sizeof(*r));
        r->is_open = 1;
        r->revision = 1U;
        strncpy(r->path, path, sizeof(r->path) - 1U);
        r->path[sizeof(r->path) - 1U] = '\0';
        return 0;
}

int cfg_repo_adjust(cfg_repo_t *r, const cfg_adjust_req_t *req)
{
        if (!r || !req) {
                return -EINVAL;
        }
        if (!r->is_open) {
                return -EACCES;
        }

        r->last_adjust = req->value;
        r->revision++;
        return 0;
}

int cfg_repo_reopen(cfg_repo_t *r)
{
        if (!r) {
                return -EINVAL;
        }
        if (r->path[0] == '\0') {
                return -ENOENT;
        }

        r->is_open = 1;
        r->revision++;
        return 0;
}

int cfg_repo_modify(cfg_repo_t *r, const cfg_modify_req_t *req)
{
        if (!r || !req) {
                return -EINVAL;
        }
        if (!r->is_open) {
                return -EACCES;
        }

        r->last_modify = req->value;
        r->revision += (uint32_t)((req->key != 0) ? 2 : 1);
        return 0;
}

int cfg_repo_close(cfg_repo_t *r)
{
        if (!r) {
                return -EINVAL;
        }

        r->is_open = 0;
        return 0;
}