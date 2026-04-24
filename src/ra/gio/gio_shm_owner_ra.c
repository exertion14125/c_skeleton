#include <stdlib.h>
#include <string.h>

#include "mgr/gio/gio_ipc_shm.h"
#include "ra/gio/gio_shm_owner_ra.h"
#include "util/ipc/shm_layout.h"

/// @brief Shared memory owner resource accessor for GIO manager. 
/// This module manages the shared memory segment used for GIO IPC, 
/// including creation, mapping, and header initialization/validation. 
/// It also provides a registry mechanism for sharing the base address 
/// and size of the shared memory with other components that need to access it.
typedef struct gio_shm_shared_registry_s {
        uint32_t valid;         ///< Indicates if the registry entry is valid
        char shm_name[64];      ///< Name of the shared memory segment
        void *base_ptr;         ///< Base address of the shared memory segment
        size_t size;            ///< Size of the shared memory segment
} gio_shm_shared_registry_t;

/// @brief GIO shared memory owner resource accessor. 
/// Manages the shared memory segment for GIO IPC,
/// including creation, mapping, and header initialization/validation.
struct gio_shm_owner_ra_s {
        gio_shm_owner_ra_cfg_t cfg; ///< Configuration for the shared memory owner RA
        gio_shm_t shm;          ///< Shared memory object representing the mapped shared memory segment
        uint32_t opened;        ///< Indicates if the shared memory segment is currently opened and mapped
};

static gio_shm_shared_registry_t g_shm_registry;

/// @brief Sets the shared memory registry entry.
/// @param name Name of the shared memory segment.
/// @param base_ptr Base address of the shared memory segment.
/// @param size Size of the shared memory segment.
static void gio_shm_owner_ra_registry_set(const char *name, void *base_ptr, size_t size)
{
        memset(&g_shm_registry, 0, sizeof(g_shm_registry));
        g_shm_registry.valid = 1U;
        strncpy(g_shm_registry.shm_name, name, sizeof(g_shm_registry.shm_name) - 1U);
        g_shm_registry.base_ptr = base_ptr;
        g_shm_registry.size = size;
}

/// @brief Clears the shared memory registry entry.
/// @param name Name of the shared memory segment. If NULL, clears the entire registry.
static void gio_shm_owner_ra_registry_clear(const char *name)
{
        if (!g_shm_registry.valid) {
                return;
        }
        if (name == NULL) {
                memset(&g_shm_registry, 0, sizeof(g_shm_registry));
                return;
        }
        if (strncmp(g_shm_registry.shm_name, name, sizeof(g_shm_registry.shm_name)) == 0) {
                memset(&g_shm_registry, 0, sizeof(g_shm_registry));
        }
}

/// @brief Retrieves the base address and size of the shared memory segment.
/// @param shm_name Name of the shared memory segment.
/// @param base_ptr Pointer to store the base address of the shared memory segment.
/// @param size Pointer to store the size of the shared memory segment.
/// @return 0 on success, -1 on failure.
int gio_shm_owner_ra_get_shared_base(const char *shm_name, void **base_ptr, size_t *size)
{
        if (shm_name == NULL || base_ptr == NULL || size == NULL) {
                return -1;
        }
        if (!g_shm_registry.valid) {
                return -1;
        }
        if (strncmp(g_shm_registry.shm_name, shm_name, sizeof(g_shm_registry.shm_name)) != 0) {
                return -1;
        }

        *base_ptr = g_shm_registry.base_ptr;
        *size = g_shm_registry.size;
        return 0;
}

/// @brief Allocates a GIO shared memory owner resource accessor.
/// @return Pointer to the allocated GIO shared memory owner RA, or NULL on failure.
gio_shm_owner_ra_t *alloc_gio_shm_owner_ra(void)
{
        return (gio_shm_owner_ra_t *)calloc(1, sizeof(gio_shm_owner_ra_t));
}

/// @brief Destroys a GIO shared memory owner resource accessor.
/// @param pra Pointer to the pointer of the GIO shared memory owner RA to be destroyed.
void destroy_gio_shm_owner_ra(gio_shm_owner_ra_t **pra)
{
        gio_shm_owner_ra_t *ra;

        if (pra == NULL || *pra == NULL) {
                return;
        }

        ra = *pra;
        *pra = NULL;

        deinit_gio_shm_owner_ra(ra);
        free(ra);
}

/// @brief Initializes a GIO shared memory owner resource accessor.
/// @param ra Pointer to the GIO shared memory owner RA to be initialized.
/// @param cfg Pointer to the configuration for the GIO shared memory owner RA.
/// @return 0 on success, -1 on failure.
int init_gio_shm_owner_ra(gio_shm_owner_ra_t *ra, const gio_shm_owner_ra_cfg_t *cfg)
{
        if (ra == NULL) {
                return -1;
        }

        memset(ra, 0, sizeof(*ra));
        if (cfg != NULL) {
                ra->cfg = *cfg;
        }

        if (ra->cfg.shm_name == NULL) {
                ra->cfg.shm_name = "/skeleton_gio_shm";
        }
        if (ra->cfg.shm_size == 0U) {
                ra->cfg.shm_size = sizeof(system_shm_t);
        }
        if (ra->cfg.layout_version == 0U) {
                ra->cfg.layout_version = SHM_LAYOUT_VERSION;
        }

        return 0;
}

/// @brief Deinitializes a GIO shared memory owner resource accessor, closing any opened shared memory segment and clearing the registry entry.
/// @param ra Pointer to the GIO shared memory owner RA to be deinitialized.
void deinit_gio_shm_owner_ra(gio_shm_owner_ra_t *ra)
{
        if (ra == NULL) {
                return;
        }
        gio_shm_owner_ra_close(ra);
}

/// @brief Opens the shared memory segment for the GIO shared memory owner RA, maps it into the process's address space, and sets the registry entry for sharing the base address and size with other components.
/// @param ra Pointer to the GIO shared memory owner RA to be opened.
/// @return 0 on success, -1 on failure.
int gio_shm_owner_ra_open(gio_shm_owner_ra_t *ra)
{
        if (ra == NULL) {
                return -1;
        }
        if (ra->opened) {
                return 0;
        }

        if (gio_shm_open(&ra->shm, ra->cfg.shm_name, ra->cfg.shm_size) != 0) {
                return -1;
        }
        if (gio_shm_map(&ra->shm) != 0) {
                (void)gio_shm_close(&ra->shm);
                return -1;
        }

        gio_shm_owner_ra_registry_set(ra->cfg.shm_name, ra->shm.mem, ra->shm.size);
        ra->opened = 1U;
        return 0;
}

/// @brief Closes the shared memory segment for the GIO shared memory owner RA, unmaps it from the process's address space, and clears the registry entry for sharing the base address and size with other components.
/// @param ra Pointer to the GIO shared memory owner RA to be closed.
void gio_shm_owner_ra_close(gio_shm_owner_ra_t *ra)
{
        if (ra == NULL || !ra->opened) {
                return;
        }

        gio_shm_owner_ra_registry_clear(ra->cfg.shm_name);
        (void)gio_shm_close(&ra->shm);
        ra->opened = 0U;
}

/// @brief Initializes the global header of the shared memory segment for the GIO shared memory owner RA. This should be called after successfully opening and mapping the shared memory segment.
/// @param ra Pointer to the GIO shared memory owner RA for which to initialize the header.
/// @return 0 on success, -1 on failure.
int gio_shm_owner_ra_init_header(gio_shm_owner_ra_t *ra)
{
        shm_global_hdr_t *hdr;

        if (ra == NULL || !ra->opened || ra->shm.mem == NULL) {
                return -1;
        }

        hdr = (shm_global_hdr_t *)ra->shm.mem;
        return shm_global_header_init(hdr, (uint32_t)ra->shm.size, ra->cfg.generation);
}

/// @brief Validates the global header of the shared memory segment for the GIO shared memory owner RA. This should be called after successfully opening and mapping the shared memory segment to ensure that the header is correctly initialized and matches the expected configuration.
/// @param ra Pointer to the GIO shared memory owner RA for which to validate the header.
/// @return 0 on success, -1 on failure, or specific error code from shm_global_header_validate if the header is invalid.
int gio_shm_owner_ra_validate_header(gio_shm_owner_ra_t *ra)
{
        shm_global_hdr_t *hdr;
        int rc;

        if (ra == NULL || !ra->opened || ra->shm.mem == NULL) {
                return -1;
        }

        hdr = (shm_global_hdr_t *)ra->shm.mem;
        rc = shm_global_header_validate(hdr,
                                        (uint32_t)ra->shm.size,
                                        ra->cfg.layout_version);
        if (rc != SHM_OK) {
                return rc;
        }

        if (hdr->generation != ra->cfg.generation) {
                return SHM_EINVAL;
        }

        return SHM_OK;
}

/// @brief Retrieves the base pointer of the shared memory segment for the GIO shared memory owner RA.
/// @param ra Pointer to the GIO shared memory owner RA.
/// @return Base pointer of the shared memory segment, or NULL if the RA is not opened.
void *gio_shm_owner_ra_get_base_ptr(const gio_shm_owner_ra_t *ra)
{
        if (ra == NULL || !ra->opened) {
                return NULL;
        }
        return ra->shm.mem;
}

/// @brief Retrieves the size of the shared memory segment for the GIO shared memory owner RA.
/// @param ra Pointer to the GIO shared memory owner RA.
/// @return Size of the shared memory segment, or 0 if the RA is not opened.
size_t gio_shm_owner_ra_get_size(const gio_shm_owner_ra_t *ra)
{
        if (ra == NULL || !ra->opened) {
                return 0U;
        }
        return ra->shm.size;
}