#include <stdlib.h>
#include <string.h>

#include "engine/cfg/cfg_engine_priv.h"

cfg_engine_t *alloc_cfg_engine(void)
{
        cfg_engine_t *e = (cfg_engine_t *)calloc(1, sizeof(*e));
        return e;
}

void destroy_cfg_engine(cfg_engine_t **pe)
{
        cfg_engine_t *e;

        if (!pe || !*pe) {
                return;
        }

        e = *pe;
        *pe = NULL;

        deinit_cfg_engine(e);
        free(e);
}

int init_cfg_engine(cfg_engine_t *e)
{
        if (!e) {
                return -1;
        }

        memset(e, 0, sizeof(*e));
        return 0;
}

void deinit_cfg_engine(cfg_engine_t *e)
{
        if (!e) {
                return;
        }

        /* 현재 동적 하위 자원 없음 */
}

int cfg_engine_apply(cfg_engine_t *e,
                     const cfg_eng_input_t *in,
                     cfg_eng_output_t *out)
{
        int rc = -1;

        if (!e || !in || !out) {
                return -1;
        }

        memset(out, 0, sizeof(*out));
        out->req_id = in->req_id;

        switch (in->kind) {
        case CFG_ENG_IN_OPEN_REQ:
                rc = cfg_repo_open(&e->repo, "/tmp/skeleton.cfg");
                e->open_count++;
                out->action = CFG_ENG_ACT_OPEN_DONE;
                out->rc = rc;
                return 0;

        case CFG_ENG_IN_ADJUST_REQ: {
                cfg_adjust_req_t req;
                req.value = in->arg0;
                rc = cfg_repo_adjust(&e->repo, &req);
                e->adjust_count++;
                out->action = CFG_ENG_ACT_ADJUST_DONE;
                out->rc = rc;
                return 0;
        }

        case CFG_ENG_IN_REOPEN_REQ:
                rc = cfg_repo_reopen(&e->repo);
                e->reopen_count++;
                out->action = CFG_ENG_ACT_REOPEN_DONE;
                out->rc = rc;
                return 0;

        case CFG_ENG_IN_MODIFY_REQ: {
                cfg_modify_req_t req;
                req.key = in->arg0;
                req.value = in->arg1;
                rc = cfg_repo_modify(&e->repo, &req);
                e->modify_count++;
                out->action = CFG_ENG_ACT_MODIFY_DONE;
                out->rc = rc;
                return 0;
        }

        case CFG_ENG_IN_TICK:
                out->action = CFG_ENG_ACT_KEEP;
                out->rc = 0;
                return 0;

        case CFG_ENG_IN_NONE:
        default:
                return -1;
        }
}