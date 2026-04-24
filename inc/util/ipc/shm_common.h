#ifndef UTIL_IPC_SHM_COMMON_H
#define UTIL_IPC_SHM_COMMON_H

#include <stdint.h>

//===== Global SHM identity and version contracts. =====//
#define SHM_GLOBAL_MAGIC    0x53484d31u /* 'SHM1' */
#define SHM_FORMAT_VERSION  1u
#define SHM_LAYOUT_VERSION  1u

/// @brief SHM operation result codes.
enum {
        SHM_OK = 0,
        SHM_EINVAL = -1,
        SHM_EBOUNDS = -2,
        SHM_ESTALE = -3
};

/// @brief SHM global header structure. Placed at the beginning of the shared memory segment for validation and metadata.
typedef struct shm_global_hdr_s {
        uint32_t magic;         //<< Magic number for validation.
        uint16_t version;       //<< Format version for compatibility check.
        uint16_t header_size;   //<< Size of the global header structure.
        uint32_t total_size;    //<< Total size of the shared memory segment including header and data.
        uint32_t layout_version;//<< Layout version for structural compatibility check.
        uint32_t generation;    //<< Generation counter for detecting restarts or reinitializations.
        uint32_t flags;         //<< Flags for future use (e.g., feature bits, status indicators).
        uint32_t reserved;      //<< Reserved for future use, should be set to 0.
} shm_global_hdr_t;

/// @brief SHM double buffer control structure. Manages the state and metadata for a double-buffered data region.
typedef struct shm_dbuf_ctrl_s {
    uint32_t active_idx;    //<< Index of the currently active buffer.
    uint32_t publish_seq;   //<< Sequence number for tracking publications.
    uint32_t payload_size;  //<< Size of the payload in the buffer.
    uint32_t flags;         //<< Flags for buffer state or attributes.
} shm_dbuf_ctrl_t;

/// @brief SHM slot header structure. Placed at the beginning of each slot in the double buffer for metadata and validation.
typedef struct shm_slot_hdr_s {
    uint32_t seq;       //<< Sequence number for the slot.
    uint32_t size;      //<< Size of the slot's payload.
    uint32_t flags;     //<< Flags for slot state or attributes.
    uint32_t reserved;  //<< Reserved for future use, should be set to 0.
} shm_slot_hdr_t;

///===== Memory barrier helpers for DBUF publish/read ordering. =====//

extern void shm_memory_barrier_release(void);
extern void shm_memory_barrier_acquire(void);
extern void shm_memory_barrier_full(void);

///===== Global header helpers. =====//

extern int shm_global_header_init(shm_global_hdr_t *hdr, uint32_t total_size, uint32_t generation);
extern int shm_global_header_validate(const shm_global_hdr_t *hdr, uint32_t expected_total_size, uint32_t expected_layout_version);

#endif /* UTIL_IPC_SHM_COMMON_H */
