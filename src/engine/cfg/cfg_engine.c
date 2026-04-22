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
                out->logic_map_valid = 1U;
                out->logic_out_card_no = (uint32_t)in->arg0;
                // out->logic_out_card_type = GIO_CARD_TYPE_DO;
                out->logic_out_ch0 = (uint32_t)in->arg1;
                out->logic_out_ch1 = ((uint32_t)in->arg1 + 1U);
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

int cfg_engine_apply_request_dto(cfg_engine_t *e,
                                 const cfg_request_dto_t *req,
                                 cfg_eng_output_t *out)
{
        int rc = 0;

        if (!e || !req || !out || !req->valid) {
                return -1;
        }

        memset(out, 0, sizeof(*out));
        out->req_id = req->version;
        out->rc = 0;

        switch (req->req_kind) {
        case CFG_REQ_KIND_OPEN:
                rc = cfg_repo_open(&e->repo, "/tmp/skeleton.cfg");
                e->open_count++;
                out->action = CFG_ENG_ACT_OPEN_DONE;
                out->rc = rc;
                return 0;

        case CFG_REQ_KIND_ADJUST: {
                cfg_adjust_req_t areq;
                areq.value = req->arg0;
                rc = cfg_repo_adjust(&e->repo, &areq);
                e->adjust_count++;
                out->action = CFG_ENG_ACT_ADJUST_DONE;
                out->rc = rc;
                out->value0 = req->arg0;
                out->value1 = req->arg1;
                return 0;
        }

        case CFG_REQ_KIND_REOPEN:
                rc = cfg_repo_reopen(&e->repo);
                e->reopen_count++;
                out->action = CFG_ENG_ACT_REOPEN_DONE;
                out->rc = rc;
                return 0;

        case CFG_REQ_KIND_MODIFY: {
                cfg_modify_req_t mreq;
                mreq.key = (int32_t)req->modify.out_card_no;
                mreq.value = (int32_t)req->modify.out_ch0;
                rc = cfg_repo_modify(&e->repo, &mreq);
                e->modify_count++;
                out->action = CFG_ENG_ACT_MODIFY_DONE;
                out->rc = rc;

                /*
                 * logic mapping 결과를 engine output으로 직접 제공
                 */
                out->value0 = (int32_t)req->modify.out_card_no;
                out->value1 = (int32_t)req->modify.out_ch0;

                out->logic_map_valid = 1U;
                out->logic_out_card_no = req->modify.out_card_no;
                out->logic_out_card_type = req->modify.out_card_type;
                out->logic_out_ch0 = req->modify.out_ch0;
                out->logic_out_ch1 = req->modify.out_ch1;
                return 0;
        }

        default:
                return -1;
        }
}