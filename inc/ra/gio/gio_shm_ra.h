#ifndef __RA_GIO_GIO_SHM_RA_H__
#define __RA_GIO_GIO_SHM_RA_H__

#include <stddef.h>
#include <stdint.h>

#include "ra/gio/gio_ipc_shm.h"
#include "ra/gio/gio_ipc_sem.h"
#include "resource/gio/gio_snapshot_dto.h"

/// @brief Configuration structure for the GIO shared memory RA, containing parameters for shared memory 
/// and semaphore names, as well as retry and timeout settings.
typedef struct gio_shm_ra_cfg_s {
        const char *shm_name;           ///< Name of the shared memory segment used for communication between GIO and IO processes.
        const char *req_sem_name;       ///< Name of the semaphore used for request synchronization between GIO and IO processes.
        const char *rsp_sem_name;       ///< Name of the semaphore used for response synchronization between GIO and IO processes.
        uint32_t req_sem_init;          ///< Initial value for the request semaphore, indicating the number of available request slots.
        uint32_t rsp_sem_init;          ///< Initial value for the response semaphore, indicating the number of available response slots.
        uint32_t max_retry;             ///< Maximum number of retry attempts for IPC operations in case of transient failures or timeouts.
        uint32_t timeout_ms;            ///< Timeout duration in milliseconds for IPC operations, after which a retry will be attempted if max_retry is not exceeded.
} gio_shm_ra_cfg_t;

/// @brief Structure representing a request for the GIO shared memory RA.
typedef struct gio_shm_ra_exec_req_s {
        uint32_t req_id;               ///< Identifier for the request.
        int32_t arg;                   ///< Argument for the request.
} gio_shm_ra_exec_req_t;

/// @brief Structure representing a response for the GIO shared memory RA.
typedef struct gio_shm_ra_exec_rsp_s {
        int32_t rc;                    ///< Return code for the request.
} gio_shm_ra_exec_rsp_t;

typedef struct gio_shm_ra_s gio_shm_ra_t;

gio_shm_ra_t *alloc_gio_shm_ra(void);
void destroy_gio_shm_ra(gio_shm_ra_t **pra);

int init_gio_shm_ra(gio_shm_ra_t *ra, const gio_shm_ra_cfg_t *cfg);
void deinit_gio_shm_ra(gio_shm_ra_t *ra);

int gio_shm_ra_set_base(gio_shm_ra_t *ra, void *base_ptr, size_t size);

int gio_shm_ra_connect(gio_shm_ra_t *ra);
void gio_shm_ra_disconnect(gio_shm_ra_t *ra);

int gio_shm_ra_exec(gio_shm_ra_t *ra,
                    const gio_shm_ra_exec_req_t *req,
                    gio_shm_ra_exec_rsp_t *rsp);

int gio_shm_ra_read_ctrl(gio_shm_ra_t *ra, gio_shm_ctrl_t *out);
int gio_shm_ra_write_ctrl(gio_shm_ra_t *ra, const gio_shm_ctrl_t *in);

int gio_shm_ra_read_input(gio_shm_ra_t *ra, gio_input_snapshot_t *out);
int gio_shm_ra_write_output(gio_shm_ra_t *ra, const gio_output_snapshot_t *in);

int gio_shm_ra_read_input_snapshot(gio_shm_ra_t *ra, gio_input_snapshot_t *out, uint32_t *out_seq);
int gio_shm_ra_write_output_snapshot(gio_shm_ra_t *ra, const gio_output_snapshot_t *in);

int gio_shm_ra_read_all(gio_shm_ra_t *ra, gio_shared_memory_t *out);
int gio_shm_ra_write_all(gio_shm_ra_t *ra, const gio_shared_memory_t *in);

#endif /* __RA_GIO_GIO_SHM_RA_H__ */