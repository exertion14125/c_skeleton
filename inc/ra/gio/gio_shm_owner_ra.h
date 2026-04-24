#ifndef __RA_GIO_GIO_SHM_OWNER_RA_H__
#define __RA_GIO_GIO_SHM_OWNER_RA_H__

#include <stddef.h>
#include <stdint.h>

typedef struct gio_shm_owner_ra_cfg_s {
        const char *shm_name;
        size_t shm_size;
        uint32_t generation;
        uint32_t layout_version;
} gio_shm_owner_ra_cfg_t;

typedef struct gio_shm_owner_ra_s gio_shm_owner_ra_t;

gio_shm_owner_ra_t *alloc_gio_shm_owner_ra(void);
void destroy_gio_shm_owner_ra(gio_shm_owner_ra_t **pra);

int init_gio_shm_owner_ra(gio_shm_owner_ra_t *ra, const gio_shm_owner_ra_cfg_t *cfg);
void deinit_gio_shm_owner_ra(gio_shm_owner_ra_t *ra);

int gio_shm_owner_ra_open(gio_shm_owner_ra_t *ra);
void gio_shm_owner_ra_close(gio_shm_owner_ra_t *ra);

int gio_shm_owner_ra_init_header(gio_shm_owner_ra_t *ra);
int gio_shm_owner_ra_validate_header(gio_shm_owner_ra_t *ra);

void *gio_shm_owner_ra_get_base_ptr(const gio_shm_owner_ra_t *ra);
size_t gio_shm_owner_ra_get_size(const gio_shm_owner_ra_t *ra);

/* Shared lookup API for non-owner RA attach path. */
int gio_shm_owner_ra_get_shared_base(const char *shm_name, void **base_ptr, size_t *size);

#endif /* __RA_GIO_GIO_SHM_OWNER_RA_H__ */