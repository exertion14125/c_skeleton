#ifndef __RA_CFG_CFG_RESULT_RA_H__
#define __RA_CFG_CFG_RESULT_RA_H__

#include "resource/cfg/cfg_result_dto.h"

typedef struct cfg_result_ra_s cfg_result_ra_t;

cfg_result_ra_t *alloc_cfg_result_ra(void);
void destroy_cfg_result_ra(cfg_result_ra_t **pra);

int init_cfg_result_ra(cfg_result_ra_t *ra);
void deinit_cfg_result_ra(cfg_result_ra_t *ra);

int cfg_result_ra_read(cfg_result_ra_t *ra, cfg_result_dto_t *out);
int cfg_result_ra_write(cfg_result_ra_t *ra, const cfg_result_dto_t *in);

#endif /* __RA_CFG_CFG_RESULT_RA_H__ */