#ifndef __RA_GIO_GIO_SHM_OWNER_RA_H__
#define __RA_GIO_GIO_SHM_OWNER_RA_H__

#include <stddef.h>
#include <stdint.h>

/// @brief Configuration structure for the GIO shared memory owner resource accessor, containing parameters for shared memory name, size, generation, and layout version.
typedef struct gio_shm_owner_ra_cfg_s {
        const char *shm_name;           ///< Name of the shared memory segment to be created and managed by the GIO shared memory owner RA. This name will be used for creating and accessing the shared memory segment.
        size_t shm_size;                ///< Size of the shared memory segment to be created and managed by the GIO shared memory owner RA. This size will determine the amount of memory allocated for the shared memory segment.
        uint32_t generation;            ///< Generation number for the shared memory segment, used for validating the header and ensuring compatibility between the owner RA and other components accessing the shared memory. This should be incremented whenever there are changes to the shared memory layout or semantics that would require other components to update their handling of the shared memory.
        uint32_t layout_version;        ///< Layout version for the shared memory segment, used for validating the header and ensuring compatibility between the owner RA and other components accessing the shared memory. This should be incremented whenever there are changes to the structure or organization of the data within the shared memory segment that would require other components to update their handling of the shared memory.
} gio_shm_owner_ra_cfg_t;

typedef struct gio_shm_owner_ra_s gio_shm_owner_ra_t;

extern gio_shm_owner_ra_t *alloc_gio_shm_owner_ra(void);
extern void destroy_gio_shm_owner_ra(gio_shm_owner_ra_t **pra);

extern int init_gio_shm_owner_ra(gio_shm_owner_ra_t *ra, const gio_shm_owner_ra_cfg_t *cfg);
extern void deinit_gio_shm_owner_ra(gio_shm_owner_ra_t *ra);

extern int gio_shm_owner_ra_open(gio_shm_owner_ra_t *ra);
extern void gio_shm_owner_ra_close(gio_shm_owner_ra_t *ra);

extern int gio_shm_owner_ra_init_header(gio_shm_owner_ra_t *ra);
extern int gio_shm_owner_ra_validate_header(gio_shm_owner_ra_t *ra);

extern void *gio_shm_owner_ra_get_base_ptr(const gio_shm_owner_ra_t *ra);
extern size_t gio_shm_owner_ra_get_size(const gio_shm_owner_ra_t *ra);

/* Shared lookup API for non-owner RA attach path. */
extern int gio_shm_owner_ra_get_shared_base(const char *shm_name, void **base_ptr, size_t *size);

#endif /* __RA_GIO_GIO_SHM_OWNER_RA_H__ */