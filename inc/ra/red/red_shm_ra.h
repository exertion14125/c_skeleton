#ifndef __RA_RED_RED_SHM_RA_H__
#define __RA_RED_RED_SHM_RA_H__

#include <stddef.h>

#include "resource/red/red_state_dto.h"

typedef struct red_shm_ra_cfg_s {
        const char *shm_name;
} red_shm_ra_cfg_t;

typedef struct red_shm_ra_s red_shm_ra_t;

red_shm_ra_t *alloc_red_shm_ra(void);
void destroy_red_shm_ra(red_shm_ra_t **pra);

int init_red_shm_ra(red_shm_ra_t *ra, const red_shm_ra_cfg_t *cfg);
void deinit_red_shm_ra(red_shm_ra_t *ra);

int red_shm_ra_set_base(red_shm_ra_t *ra, void *base_ptr, size_t size);
int red_shm_ra_connect(red_shm_ra_t *ra);
void red_shm_ra_disconnect(red_shm_ra_t *ra);

int red_shm_ra_write_state(red_shm_ra_t *ra, const red_state_dto_t *in);
int red_shm_ra_read_state(red_shm_ra_t *ra, red_state_dto_t *out);

#endif /* __RA_RED_RED_SHM_RA_H__ */