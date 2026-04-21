#include <stdlib.h>
#include <string.h>

#include "engine/gio/gio_engine_priv.h"

gio_engine_t *alloc_gio_engine(void)
{
        gio_engine_t *e = (gio_engine_t *)calloc(1, sizeof(*e));
        return e;
}

void destroy_gio_engine(gio_engine_t **pe)
{
        gio_engine_t *e;

        if (!pe || !*pe) {
                return;
        }

        e = *pe;
        *pe = NULL;

        deinit_gio_engine(e);
        free(e);
}

int init_gio_engine(gio_engine_t *e)
{
        if (!e) {
                return -1;
        }

        memset(e, 0, sizeof(*e));
        return 0;
}

void deinit_gio_engine(gio_engine_t *e)
{
        if (!e) {
                return;
        }

        /* 현재 동적 하위 자원 없음 */
}

int gio_engine_apply(gio_engine_t *e,
                     const gio_eng_input_t *in,
                     gio_eng_output_t *out)
{
        if (!e || !in || !out) {
                return -1;
        }

        memset(out, 0, sizeof(*out));
        out->req_id = in->req_id;

        switch (in->kind) {
        case GIO_ENG_IN_EXEC_RESULT:
                e->last_exec_ms = in->now_ms;
                e->last_rc = in->raw_rc;

                if (in->raw_rc == 0) {
                        e->exec_count++;
                        out->action = GIO_ENG_ACT_EXEC_DONE;
                        out->rc = 0;
                        return 0;
                }

                e->timeout_count++;
                e->degraded_count++;
                out->action = GIO_ENG_ACT_TIMEOUT_EVT;
                out->rc = in->raw_rc;
                out->value0 = (int32_t)e->timeout_count;
                return 0;

        case GIO_ENG_IN_TICK:
                out->action = GIO_ENG_ACT_KEEP;
                return 0;

        case GIO_ENG_IN_NONE:
        default:
                return -1;
        }
}

int gio_engine_apply_snapshot(gio_engine_t *e,
                              const gio_shm_ctrl_t *ctrl,
                              const gio_input_snapshot_t *input,
                              uint64_t now_ms,
                              gio_eng_output_t *out)
{
        if (!e || !ctrl || !input || !out) {
                return -1;
        }

        memset(out, 0, sizeof(*out));
        out->req_id = ctrl->req_epoch;

        e->last_req_epoch = ctrl->req_epoch;
        e->last_rsp_epoch = ctrl->rsp_epoch;
        e->last_rsp_ts_ms = ctrl->rsp_ts_ms;
        e->last_global_err_flags = input->global_err_flags;
        e->last_global_err_count = input->global_err_count;
        e->last_exec_ms = now_ms;

        /*
         * timeout 판단:
         *   manager가 deadline check 시점에 이 함수를 호출한다고 가정.
         *   즉 지금 시점에 rsp_epoch != req_epoch 이면 timeout으로 본다.
         */
        if (ctrl->rsp_epoch != ctrl->req_epoch) {
                e->timeout_count++;
                e->degraded_count++;
                out->action = GIO_ENG_ACT_TIMEOUT_EVT;
                out->rc = -1;
                out->value0 = (int32_t)e->timeout_count;
                return 0;
        }

        /*
         * degraded 판단:
         *   global error flag가 있으면 degraded로 본다.
         *   이후 카드별 err_flags 집계로 고도화 가능.
         */
        if (input->global_err_flags != 0U) {
                e->degraded_count++;
                out->action = GIO_ENG_ACT_DEGRADED_EVT;
                out->rc = 0;
                out->value0 = (int32_t)input->global_err_flags;
                return 0;
        }

        /*
         * 정상 수신 완료
         */
        e->exec_count++;
        out->action = GIO_ENG_ACT_EXEC_DONE;
        out->rc = 0;
        out->value0 = 0;
        return 0;
}