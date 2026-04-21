#include <stdlib.h>
#include <string.h>

#include "engine/logic/logic_output_map_priv.h"

logic_output_map_t *alloc_logic_output_map(void)
{
        logic_output_map_t *m = (logic_output_map_t *)calloc(1, sizeof(*m));
        return m;
}

void destroy_logic_output_map(logic_output_map_t **pm)
{
        logic_output_map_t *m;

        if (!pm || !*pm) {
                return;
        }

        m = *pm;
        *pm = NULL;

        deinit_logic_output_map(m);
        free(m);
}

int init_logic_output_map(logic_output_map_t *m)
{
        if (!m) {
                return -1;
        }

        memset(m, 0, sizeof(*m));
        m->out_card_no = 0U;
        m->out_card_type = GIO_CARD_TYPE_DO;
        m->out_ch0 = 0U;
        m->out_ch1 = 1U;

        return 0;
}

void deinit_logic_output_map(logic_output_map_t *m)
{
        if (!m) {
                return;
        }

        /* 현재 동적 하위 자원 없음 */
}

int logic_output_map_apply(logic_output_map_t *m,
                           const logic_eng_output_t *logic_out,
                           uint64_t now_ms,
                           gio_output_snapshot_t *out_snap)
{
        if (!m || !logic_out || !out_snap) {
                return -1;
        }

        memset(out_snap, 0, sizeof(*out_snap));

        out_snap->apply_epoch = logic_out->req_id;
        out_snap->card_count = 1U;
        out_snap->ts_ms = now_ms;

        out_snap->cards[0].valid = 1U;
        out_snap->cards[0].card_no = m->out_card_no;
        out_snap->cards[0].card_type = m->out_card_type;
        out_snap->cards[0].channel_count = 2U;
        out_snap->cards[0].ts_ms = now_ms;

        if (m->out_ch0 < GIO_MAX_IO_PER_CARD) {
                out_snap->cards[0].value[m->out_ch0] = logic_out->value0;
        }
        if (m->out_ch1 < GIO_MAX_IO_PER_CARD) {
                out_snap->cards[0].value[m->out_ch1] = logic_out->value1;
        }

        return 0;
}