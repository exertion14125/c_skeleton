#ifndef __RA_LOGIC_LOGIC_CFG_RA_H__
#define __RA_LOGIC_LOGIC_CFG_RA_H__

#include "resource/logic/logic_cfg_dto.h"

typedef struct logic_cfg_ra_s logic_cfg_ra_t;

logic_cfg_ra_t *alloc_logic_cfg_ra(void);
void destroy_logic_cfg_ra(logic_cfg_ra_t **pra);

int init_logic_cfg_ra(logic_cfg_ra_t *ra);
void deinit_logic_cfg_ra(logic_cfg_ra_t *ra);

int logic_cfg_ra_read(logic_cfg_ra_t *ra, logic_cfg_dto_t *out);
int logic_cfg_ra_write(logic_cfg_ra_t *ra, const logic_cfg_dto_t *in);

#endif /* __RA_LOGIC_LOGIC_CFG_RA_H__ */