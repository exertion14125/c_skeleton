#ifndef UTIL_IPC_SHM_COMMON_H
#define UTIL_IPC_SHM_COMMON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Global SHM identity and version contract. */
#define SHM_GLOBAL_MAGIC    0x53484d31u /* 'SHM1' */
#define SHM_FORMAT_VERSION  1u
#define SHM_LAYOUT_VERSION  1u

/* Common return codes for SHM helpers. */
enum {
    SHM_OK = 0,
    SHM_EINVAL = -1,
    SHM_EBOUNDS = -2,
    SHM_ESTALE = -3
};

typedef struct shm_global_hdr_s {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t total_size;
    uint32_t layout_version;
    uint32_t generation;
    uint32_t flags;
    uint32_t reserved;
} shm_global_hdr_t;

typedef struct shm_dbuf_ctrl_s {
    uint32_t active_idx;
    uint32_t publish_seq;
    uint32_t payload_size;
    uint32_t flags;
} shm_dbuf_ctrl_t;

typedef struct shm_slot_hdr_s {
    uint32_t seq;
    uint32_t size;
    uint32_t flags;
    uint32_t reserved;
} shm_slot_hdr_t;

/* Memory barrier helpers for DBUF publish/read ordering. */
void shm_memory_barrier_release(void);
void shm_memory_barrier_acquire(void);
void shm_memory_barrier_full(void);

/* Global header helpers. */
int shm_global_header_init(shm_global_hdr_t *hdr, uint32_t total_size, uint32_t generation);
int shm_global_header_validate(const shm_global_hdr_t *hdr, uint32_t expected_total_size, uint32_t expected_layout_version);

#ifdef __cplusplus
}
#endif

#endif /* UTIL_IPC_SHM_COMMON_H */
