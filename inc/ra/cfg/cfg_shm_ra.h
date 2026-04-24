#ifndef __RA_CFG_CFG_SHM_RA_H__
#define __RA_CFG_CFG_SHM_RA_H__

#include <stddef.h>
#include <stdint.h>

#include "resource/cfg/dto/cfg_export_dto.h"

typedef struct cfg_shm_ra_cfg_s {
        const char *shm_name;
} cfg_shm_ra_cfg_t;

typedef struct cfg_shm_ra_s cfg_shm_ra_t;

cfg_shm_ra_t *alloc_cfg_shm_ra(void);
void destroy_cfg_shm_ra(cfg_shm_ra_t **pra);

int init_cfg_shm_ra(cfg_shm_ra_t *ra, const cfg_shm_ra_cfg_t *cfg);
void deinit_cfg_shm_ra(cfg_shm_ra_t *ra);

int cfg_shm_ra_set_base(cfg_shm_ra_t *ra, void *base_ptr, size_t size);
int cfg_shm_ra_connect(cfg_shm_ra_t *ra);
void cfg_shm_ra_disconnect(cfg_shm_ra_t *ra);

int cfg_shm_ra_write_effective_cfg(cfg_shm_ra_t *ra, const cfg_export_dto_t *in);
int cfg_shm_ra_read_effective_cfg(cfg_shm_ra_t *ra, cfg_export_dto_t *out);

#endif /* __RA_CFG_CFG_SHM_RA_H__ */