#ifndef __MGR_GIO_GIO_IPC_SEM_H__
#define __MGR_GIO_GIO_IPC_SEM_H__

#include <stdint.h>
#include <semaphore.h>

typedef struct gio_sem_cfg_s {
        const char *req_name;
        const char *rsp_name;
        uint32_t req_init;
        uint32_t rsp_init;
} gio_sem_cfg_t;

typedef struct gio_sem_s {
        sem_t *req;
        sem_t *rsp;
        char req_name[64];
        char rsp_name[64];
} gio_sem_t;

extern int gio_sem_open(gio_sem_t *s, const gio_sem_cfg_t *cfg);
extern int gio_sem_post_req(gio_sem_t *s);
extern int gio_sem_wait_rsp(gio_sem_t *s, uint32_t timeout_ms);
extern int gio_sem_close(gio_sem_t *s);

#endif /* __MGR_GIO_GIO_IPC_SEM_H__ */