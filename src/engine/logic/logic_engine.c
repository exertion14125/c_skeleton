#include <stdlib.h>
#include <string.h>

#include "engine/logic/logic_engine_priv.h"

logic_engine_t *alloc_logic_engine(void)
{
        logic_engine_t *e = (logic_engine_t *)calloc(1, sizeof(*e));
        return e;
}

void destroy_logic_engine(logic_engine_t **pe)
{
        logic_engine_t *e;

        if (!pe || !*pe) {
                return;
        }

        e = *pe;
        *pe = NULL;

        deinit_logic_engine(e);
        free(e);
}

int init_logic_engine(logic_engine_t *e)
{
        if (!e) {
                return -1;
        }

        memset(e, 0, sizeof(*e));
        return 0;
}

void deinit_logic_engine(logic_engine_t *e)
{
        if (!e) {
                return;
        }

        /* 현재 동적 하위 자원 없음 */
}

int logic_engine_apply(logic_engine_t *e,
                       const logic_eng_input_t *in,
                       logic_eng_output_t *out)
{
        if (!e || !in || !out) {
                return -1;
        }

        memset(out, 0, sizeof(*out));
        out->req_id = in->req_id;

        switch (in->kind) {
        case LOGIC_ENG_IN_EXEC_REQ:
                e->exec_count++;
                e->last_exec_ms = in->now_ms;
                e->last_rc = 0;

                out->action = LOGIC_ENG_ACT_EXEC_DONE;
                out->rc = 0;
                out->value0 = in->arg0;
                out->value1 = in->arg1;
                return 0;

        case LOGIC_ENG_IN_TICK:
                out->action = LOGIC_ENG_ACT_KEEP;
                return 0;

        case LOGIC_ENG_IN_NONE:
        default:
                return -1;
        }
}

int logic_engine_apply_snapshot(logic_engine_t *e,
                                const gio_input_snapshot_t *in,
                                uint64_t now_ms,
                                logic_eng_output_t *out)
{
        uint32_t i;
        int32_t v0 = 0;
        int32_t v1 = 0;
        uint32_t found = 0U;

        if (!e || !in || !out) {
                return -1;
        }

        memset(out, 0, sizeof(*out));
        out->req_id = in->snap_epoch;

        for (i = 0; i < in->card_count && i < GIO_MAX_CARDS; ++i) {
                if (!in->cards[i].valid) {
                        continue;
                }
                if (in->cards[i].channel_count == 0U) {
                        continue;
                }

                v0 = in->cards[i].value[0];
                if (in->cards[i].channel_count > 1U) {
                        v1 = in->cards[i].value[1];
                }
                found = 1U;
                break;
        }

        e->exec_count++;
        e->last_exec_ms = now_ms;
        e->last_rc = 0;

        out->action = LOGIC_ENG_ACT_EXEC_DONE;
        out->rc = found ? 0 : 0;
        out->value0 = v0;
        out->value1 = v1;

        return 0;
}