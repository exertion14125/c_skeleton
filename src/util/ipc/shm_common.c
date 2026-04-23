#include "util/ipc/shm_common.h"

#include <stddef.h>

void shm_memory_barrier_release(void)
{
#if defined(__GNUC__) || defined(__clang__)
    __atomic_thread_fence(__ATOMIC_RELEASE);
#else
    __sync_synchronize();
#endif
}

void shm_memory_barrier_acquire(void)
{
#if defined(__GNUC__) || defined(__clang__)
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
#else
    __sync_synchronize();
#endif
}

void shm_memory_barrier_full(void)
{
#if defined(__GNUC__) || defined(__clang__)
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#else
    __sync_synchronize();
#endif
}

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
