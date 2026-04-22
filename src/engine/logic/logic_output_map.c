#include <stdlib.h>
#include <string.h>

#include "engine/logic/logic_output_map_priv.h"

static int logic_output_map_find_or_create_card_slot(gio_output_snapshot_t *out_snap,
                                                     uint32_t card_no,
                                                     uint32_t card_type,
                                                     uint64_t now_ms)
{
        uint32_t i;

        if (!out_snap) {
                return -1;
        }

        for (i = 0; i < out_snap->card_count && i < GIO_MAX_CARDS; ++i) {
                if (out_snap->cards[i].valid &&
                    out_snap->cards[i].card_no == card_no) {
                        return (int)i;
                }
        }

        if (out_snap->card_count >= GIO_MAX_CARDS) {
                return -1;
        }

        i = out_snap->card_count++;
        memset(&out_snap->cards[i], 0, sizeof(out_snap->cards[i]));
        out_snap->cards[i].valid = 1U;
        out_snap->cards[i].card_no = card_no;
        out_snap->cards[i].card_type = card_type;
        out_snap->cards[i].ts_ms = now_ms;
        out_snap->cards[i].channel_count = 0U;

        return (int)i;
}

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

int logic_output_map_apply_cfg(logic_output_map_t *m,
                               const logic_output_map_cfg_t *cfg)
{
        uint32_t n;

        if (!m || !cfg) {
                return -1;
        }

        memset(m, 0, sizeof(*m));

        n = cfg->route_count;
        if (n > LOGIC_OUTPUT_ROUTE_MAX) {
                n = LOGIC_OUTPUT_ROUTE_MAX;
        }

        m->route_count = n;
        memcpy(m->routes, cfg->routes, sizeof(logic_output_route_cfg_t) * n);

        return 0;
}

int init_logic_output_map(logic_output_map_t *m, const logic_output_map_cfg_t *cfg)
{
        logic_output_map_cfg_t defcfg;

        if (!m) {
                return -1;
        }

        memset(m, 0, sizeof(*m));

        if (cfg) {
                return logic_output_map_apply_cfg(m, cfg);
        }

        memset(&defcfg, 0, sizeof(defcfg));
        defcfg.route_count = 2U;

        defcfg.routes[0].valid = 1U;
        defcfg.routes[0].out_card_no = 0U;
        defcfg.routes[0].out_card_type = GIO_CARD_TYPE_DO;
        defcfg.routes[0].out_value_index = 0U;
        defcfg.routes[0].out_ch = 0U;

        defcfg.routes[1].valid = 1U;
        defcfg.routes[1].out_card_no = 0U;
        defcfg.routes[1].out_card_type = GIO_CARD_TYPE_DO;
        defcfg.routes[1].out_value_index = 1U;
        defcfg.routes[1].out_ch = 1U;

        return logic_output_map_apply_cfg(m, &defcfg);
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
        uint32_t i;

        if (!m || !logic_out || !out_snap) {
                return -1;
        }

        memset(out_snap, 0, sizeof(*out_snap));
        out_snap->apply_epoch = logic_out->req_id;
        out_snap->ts_ms = now_ms;

        for (i = 0; i < m->route_count && i < LOGIC_OUTPUT_ROUTE_MAX; ++i) {
                const logic_output_route_cfg_t *r = &m->routes[i];
                int slot;
                int32_t value;

                if (!r->valid) {
                        continue;
                }
                if (r->out_ch >= GIO_MAX_IO_PER_CARD) {
                        continue;
                }
                if (r->out_value_index >= logic_out->value_count ||
                    r->out_value_index >= LOGIC_OUTPUT_VALUE_MAX) {
                        continue;
                }

                value = logic_out->values[r->out_value_index];

                slot = logic_output_map_find_or_create_card_slot(out_snap,
                                                                 r->out_card_no,
                                                                 r->out_card_type,
                                                                 now_ms);
                if (slot < 0) {
                        continue;
                }

                out_snap->cards[slot].value[r->out_ch] = value;

                if (out_snap->cards[slot].channel_count <= r->out_ch) {
                        out_snap->cards[slot].channel_count = r->out_ch + 1U;
                }
        }

        return 0;
}