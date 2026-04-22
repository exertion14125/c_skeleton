#include <stdlib.h>
#include <string.h>

#include "ra/cfg/cfg_request_ra_priv.h"

cfg_request_ra_t *alloc_cfg_request_ra(void)
{
        cfg_request_ra_t *ra = (cfg_request_ra_t *)calloc(1, sizeof(*ra));
        return ra;
}

void destroy_cfg_request_ra(cfg_request_ra_t **pra)
{
        cfg_request_ra_t *ra;

        if (!pra || !*pra) {
                return;
        }

        ra = *pra;
        *pra = NULL;

        deinit_cfg_request_ra(ra);
        free(ra);
}

int init_cfg_request_ra(cfg_request_ra_t *ra)
{
        if (!ra) {
                return -1;
        }

        memset(ra, 0, sizeof(*ra));
        return 0;
}

void deinit_cfg_request_ra(cfg_request_ra_t *ra)
{
        if (!ra) {
                return;
        }
}

int cfg_request_ra_read(cfg_request_ra_t *ra, cfg_request_dto_t *out)
{
        if (!ra || !out) {
                return -1;
        }

        *out = ra->dto;
        return 0;
}

int cfg_request_ra_write(cfg_request_ra_t *ra, const cfg_request_dto_t *in)
{
        if (!ra || !in) {
                return -1;
        }

        ra->dto = *in;
        return 0;
}