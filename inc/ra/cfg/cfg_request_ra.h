#ifndef __RA_CFG_CFG_REQUEST_RA_H__
#define __RA_CFG_CFG_REQUEST_RA_H__

#include "resource/cfg/dto/cfg_request_dto.h"

typedef struct cfg_request_ra_s cfg_request_ra_t;

cfg_request_ra_t *alloc_cfg_request_ra(void);
void destroy_cfg_request_ra(cfg_request_ra_t **pra);

int init_cfg_request_ra(cfg_request_ra_t *ra);
void deinit_cfg_request_ra(cfg_request_ra_t *ra);

int cfg_request_ra_read(cfg_request_ra_t *ra, cfg_request_dto_t *out);
int cfg_request_ra_write(cfg_request_ra_t *ra, const cfg_request_dto_t *in);

#endif /* __RA_CFG_CFG_REQUEST_RA_H__ */