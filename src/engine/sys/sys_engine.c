#include <stdlib.h>
#include <string.h>

#include "engine/sys/sys_engine_priv.h"

sys_engine_t *alloc_sys_engine(void)
{
        sys_engine_t *e = (sys_engine_t *)calloc(1, sizeof(*e));
        return e;
}

void destroy_sys_engine(sys_engine_t **pe)
{
        sys_engine_t *e;

        if (!pe || !*pe) {
                return;
        }

        e = *pe;
        *pe = NULL;

        deinit_sys_engine(e);
        free(e);
}

int init_sys_engine(sys_engine_t *e)
{
        if (!e) {
                return -1;
        }

        memset(e, 0, sizeof(*e));

        e->allow_failover = 1U;
        e->allow_recover = 1U;
        e->hold_flag = 0U;

        return 0;
}

void deinit_sys_engine(sys_engine_t *e)
{
        if (!e) {
                return;
        }

        /* 현재 동적 하위 자원 없음 */
}

int sys_engine_apply(sys_engine_t *e,
                     const sys_eng_input_t *in,
                     sys_eng_output_t *out)
{
        if (!e || !in || !out) {
                return -1;
        }

        memset(out, 0, sizeof(*out));
        out->req_id = in->req_id;

        switch (in->kind) {
        case SYS_ENG_IN_BOOTSTRAP:
                switch (e->bootstrap_step) {
                case 0U:
                        out->action = SYS_ENG_ACT_SEND_CFG_OPEN;
                        out->req_id = 1U;
                        e->bootstrap_step = 1U;
                        return 0;

                case 1U:
                        out->action = SYS_ENG_ACT_SEND_GIO_EXEC;
                        out->req_id = 2U;
                        out->value0 = 0;
                        e->bootstrap_step = 2U;
                        return 0;

                case 2U:
                        out->action = SYS_ENG_ACT_SEND_RED_EVAL;
                        out->req_id = 3U;
                        e->bootstrap_step = 3U;
                        return 0;

                default:
                        out->action = SYS_ENG_ACT_KEEP;
                        return 0;
                }

        case SYS_ENG_IN_CFG_RSP:
                e->cfg_rsp_count++;
                out->action = SYS_ENG_ACT_CFG_ACCEPTED;
                return 0;

        case SYS_ENG_IN_GIO_RSP:
                e->gio_rsp_count++;
                out->action = SYS_ENG_ACT_GIO_ACCEPTED;
                return 0;

        case SYS_ENG_IN_RED_RSP:
                e->red_rsp_count++;
                out->action = SYS_ENG_ACT_RED_ACCEPTED;
                return 0;

        case SYS_ENG_IN_RED_PROPOSE_FAILOVER:
                if (e->hold_flag) {
                        out->action = SYS_ENG_ACT_REJECT_FAILOVER;
                        out->reason_code = 1U;
                        return 0;
                }
                if (!e->allow_failover) {
                        out->action = SYS_ENG_ACT_REJECT_FAILOVER;
                        out->reason_code = 2U;
                        return 0;
                }
                out->action = SYS_ENG_ACT_APPROVE_FAILOVER;
                return 0;

        case SYS_ENG_IN_RED_PROPOSE_RECOVER:
                if (e->hold_flag) {
                        out->action = SYS_ENG_ACT_REJECT_RECOVER;
                        out->reason_code = 1U;
                        return 0;
                }
                if (!e->allow_recover) {
                        out->action = SYS_ENG_ACT_REJECT_RECOVER;
                        out->reason_code = 2U;
                        return 0;
                }
                out->action = SYS_ENG_ACT_APPROVE_RECOVER;
                return 0;

        case SYS_ENG_IN_RED_PROPOSE_HOLD:
                e->hold_flag = 1U;
                out->action = SYS_ENG_ACT_ENTER_HOLD;
                return 0;

        case SYS_ENG_IN_GIO_RX_DONE:
                e->gio_rx_done_count++;
                out->action = SYS_ENG_ACT_LOGIC_READY;
                out->req_id = in->req_id;
                out->value0 = in->arg0;
                out->value1 = in->arg1;
                return 0;

        case SYS_ENG_IN_LOGIC_RSP:
                e->logic_rsp_count++;
                out->action = SYS_ENG_ACT_KEEP;
                out->req_id = in->req_id;
                out->value0 = in->arg0;
                out->value1 = in->arg1;
                return 0;

        case SYS_ENG_IN_TICK:
                out->action = SYS_ENG_ACT_KEEP;
                return 0;

        case SYS_ENG_IN_ERROR:
                e->err_flag = 1U;
                e->last_reason_code = (uint32_t)in->arg0;
                out->action = SYS_ENG_ACT_ENTER_ERR;
                out->reason_code = e->last_reason_code;
                return 0;

        case SYS_ENG_IN_NONE:
        default:
                return -1;
        }
}