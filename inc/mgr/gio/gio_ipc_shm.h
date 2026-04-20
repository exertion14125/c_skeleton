#ifndef __MGR_GIO_GIO_IPC_SHM_H__
#define __MGR_GIO_GIO_IPC_SHM_H__

#include <stddef.h>
#include <stdint.h>

typedef struct gio_ipc_req_s {
        uint32_t req_id;
        int32_t arg;
} gio_ipc_req_t;

typedef struct gio_ipc_rsp_s {
        uint32_t req_id;
        int32_t rc;
        int32_t data;
} gio_ipc_rsp_t;

typedef struct gio_shm_s {
        int fd;
        size_t size;
        char name[64];
        void *mem;
} gio_shm_t;

extern int gio_shm_open(gio_shm_t *s, const char *name, size_t size);
extern int gio_shm_map(gio_shm_t *s);
extern int gio_shm_write_req(gio_shm_t *s, const gio_ipc_req_t *req);
extern int gio_shm_read_rsp(gio_shm_t *s, gio_ipc_rsp_t *rsp);
extern int gio_shm_unmap(gio_shm_t *s);
extern int gio_shm_close(gio_shm_t *s);

#endif /* __MGR_GIO_GIO_IPC_SHM_H__ */