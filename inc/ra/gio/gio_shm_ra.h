#ifndef __RA_GIO_GIO_SHM_RA_H__
#define __RA_GIO_GIO_SHM_RA_H__

#include <stdint.h>

#include "mgr/gio/gio_ipc_shm.h"
#include "mgr/gio/gio_ipc_sem.h"
#include "resource/gio/gio_snapshot_dto.h"

typedef struct gio_shm_ra_cfg_s {
        const char *shm_name;
        const char *req_sem_name;
        const char *rsp_sem_name;
        uint32_t req_sem_init;
        uint32_t rsp_sem_init;
        uint32_t max_retry;
        uint32_t timeout_ms;
} gio_shm_ra_cfg_t;

typedef struct gio_shm_ra_exec_req_s {
        uint32_t req_id;
        int32_t arg;
} gio_shm_ra_exec_req_t;

typedef struct gio_shm_ra_exec_rsp_s {
        int32_t rc;
} gio_shm_ra_exec_rsp_t;

typedef struct gio_shm_ra_s gio_shm_ra_t;

gio_shm_ra_t *alloc_gio_shm_ra(void);
void destroy_gio_shm_ra(gio_shm_ra_t **pra);

int init_gio_shm_ra(gio_shm_ra_t *ra, const gio_shm_ra_cfg_t *cfg);
void deinit_gio_shm_ra(gio_shm_ra_t *ra);

int gio_shm_ra_connect(gio_shm_ra_t *ra);
void gio_shm_ra_disconnect(gio_shm_ra_t *ra);

int gio_shm_ra_exec(gio_shm_ra_t *ra,
                    const gio_shm_ra_exec_req_t *req,
                    gio_shm_ra_exec_rsp_t *rsp);

int gio_shm_ra_read_ctrl(gio_shm_ra_t *ra, gio_shm_ctrl_t *out);
int gio_shm_ra_write_ctrl(gio_shm_ra_t *ra, const gio_shm_ctrl_t *in);

int gio_shm_ra_read_input(gio_shm_ra_t *ra, gio_input_snapshot_t *out);
int gio_shm_ra_write_output(gio_shm_ra_t *ra, const gio_output_snapshot_t *in);

int gio_shm_ra_read_all(gio_shm_ra_t *ra, gio_shared_memory_t *out);
int gio_shm_ra_write_all(gio_shm_ra_t *ra, const gio_shared_memory_t *in);

#endif /* __RA_GIO_GIO_SHM_RA_H__ */