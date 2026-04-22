#include <stdlib.h>
#include <string.h>

#include "ra/cfg/cfg_result_ra_priv.h"

cfg_result_ra_t *alloc_cfg_result_ra(void)
{
        cfg_result_ra_t *ra = (cfg_result_ra_t *)calloc(1, sizeof(*ra));
        return ra;
}

void destroy_cfg_result_ra(cfg_result_ra_t **pra)
{
        cfg_result_ra_t *ra;

        if (!pra || !*pra) {
                return;
        }

        ra = *pra;
        *pra = NULL;

        deinit_cfg_result_ra(ra);
        free(ra);
}

int init_cfg_result_ra(cfg_result_ra_t *ra)
{
        if (!ra) {
                return -1;
        }

        memset(ra, 0, sizeof(*ra));
        return 0;
}

void deinit_cfg_result_ra(cfg_result_ra_t *ra)
{
        if (!ra) {
                return;
        }

        /* 현재 동적 하위 자원 없음 */
}

int cfg_result_ra_read(cfg_result_ra_t *ra, cfg_result_dto_t *out)
{
        if (!ra || !out) {
                return -1;
        }

        *out = ra->dto;
        return 0;
}

int cfg_result_ra_write(cfg_result_ra_t *ra, const cfg_result_dto_t *in)
{
        if (!ra || !in) {
                return -1;
        }

        ra->dto = *in;
        return 0;
}