#include <stdlib.h>
#include <string.h>

#include "ra/logic/logic_cfg_ra_priv.h"

logic_cfg_ra_t *alloc_logic_cfg_ra(void)
{
        logic_cfg_ra_t *ra = (logic_cfg_ra_t *)calloc(1, sizeof(*ra));
        return ra;
}

void destroy_logic_cfg_ra(logic_cfg_ra_t **pra)
{
        logic_cfg_ra_t *ra;

        if (!pra || !*pra) {
                return;
        }

        ra = *pra;
        *pra = NULL;

        deinit_logic_cfg_ra(ra);
        free(ra);
}

int init_logic_cfg_ra(logic_cfg_ra_t *ra)
{
        if (!ra) {
                return -1;
        }

        memset(ra, 0, sizeof(*ra));
        return 0;
}

void deinit_logic_cfg_ra(logic_cfg_ra_t *ra)
{
        if (!ra) {
                return;
        }
}

int logic_cfg_ra_read(logic_cfg_ra_t *ra, logic_cfg_dto_t *out)
{
        if (!ra || !out) {
                return -1;
        }

        *out = ra->dto;
        return 0;
}

int logic_cfg_ra_write(logic_cfg_ra_t *ra, const logic_cfg_dto_t *in)
{
        if (!ra || !in) {
                return -1;
        }

        ra->dto = *in;
        return 0;
}