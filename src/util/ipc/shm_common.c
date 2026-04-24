#include "util/ipc/shm_common.h"

#include <stddef.h>

/// @brief SHM memory barrier for release semantics. 
/// Ensures that all prior writes to shared memory are visible to other threads/processes before any subsequent writes.
void shm_memory_barrier_release(void)
{
#if defined(__GNUC__) || defined(__clang__)
        __atomic_thread_fence(__ATOMIC_RELEASE);
#else
        __sync_synchronize();
#endif
}

/// @brief SHM memory barrier for acquire semantics.
/// Ensures that all subsequent reads from shared memory see the effects of prior writes by other threads/processes before any subsequent reads.
void shm_memory_barrier_acquire(void)
{
#if defined(__GNUC__) || defined(__clang__)
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
#else
        __sync_synchronize();
#endif
}

/// @brief SHM memory barrier for sequential consistency. Provides the strongest ordering guarantees, ensuring that all threads/processes see all memory operations in the same order.
void shm_memory_barrier_full(void)
{
#if defined(__GNUC__) || defined(__clang__)
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
#else
        __sync_synchronize();
#endif
}

/// @brief Initialize the SHM global header with the specified total size and generation. Sets the magic number, version, layout version, and initializes flags and reserved fields.
/// @param hdr Pointer to the global header structure to initialize.
/// @param total_size Total size of the shared memory segment including header and data.
/// @param generation Generation counter for detecting restarts or reinitializations.
/// @return SHM_OK on success, or a negative error code on failure (e.g., SHM_EINVAL if parameters are invalid).
int shm_global_header_init(shm_global_hdr_t *hdr, uint32_t total_size, uint32_t generation)
{
        if (hdr == NULL || total_size < (uint32_t)sizeof(*hdr)) {
                return SHM_EINVAL;
        }

        hdr->magic = SHM_GLOBAL_MAGIC;
        hdr->version = (uint16_t)SHM_FORMAT_VERSION;
        hdr->header_size = (uint16_t)sizeof(*hdr);
        hdr->total_size = total_size;
        hdr->layout_version = SHM_LAYOUT_VERSION;
        hdr->generation = generation;
        hdr->flags = 0u;
        hdr->reserved = 0u;

        shm_memory_barrier_full();
        return SHM_OK;
}

/// @brief Validate the SHM global header against expected total size and layout version. Checks the magic number, version, header size, total size, and layout version for consistency.
/// @param hdr Pointer to the global header structure to validate.
/// @param expected_total_size Expected total size of the shared memory segment for validation.
/// @param expected_layout_version Expected layout version for validation.
/// @return SHM_OK if the header is valid, or a negative error code on failure (e.g., SHM_EINVAL for invalid header, SHM_EBOUNDS for size mismatch)
int shm_global_header_validate(const shm_global_hdr_t *hdr, uint32_t expected_total_size, uint32_t expected_layout_version)
{
        if (hdr == NULL) {
                return SHM_EINVAL;
        }

        if (hdr->magic != SHM_GLOBAL_MAGIC) {
                return SHM_EINVAL;
        }
        if (hdr->version != (uint16_t)SHM_FORMAT_VERSION) {
                return SHM_EINVAL;
        }
        if (hdr->header_size != (uint16_t)sizeof(*hdr)) {
                return SHM_EINVAL;
        }
        if (hdr->total_size != expected_total_size) {
                return SHM_EBOUNDS;
        }
        if (hdr->layout_version != expected_layout_version) {
                return SHM_EINVAL;
        }

        return SHM_OK;
}
