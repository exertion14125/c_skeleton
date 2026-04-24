#ifndef __MGR_GIO_GIO_IPC_SEM_H__
#define __MGR_GIO_GIO_IPC_SEM_H__

#include <stdint.h>
#include <semaphore.h>

/// @brief Configuration structure for initializing a GIO IPC semaphore pair. 
/// Contains the names and initial values for the request and response semaphores.
typedef struct gio_sem_cfg_s {
        const char *req_name;   ///< Name of the request semaphore (must be a null-terminated string).
        const char *rsp_name;   ///< Name of the response semaphore (must be a null-terminated string).
        uint32_t req_init;      ///< Initial value for the request semaphore (number of available request slots).
        uint32_t rsp_init;      ///< Initial value for the response semaphore (number of available response slots).
} gio_sem_cfg_t;

/// @brief Structure representing a pair of semaphores used for GIO IPC synchronization.
typedef struct gio_sem_s {
        sem_t *req;     ///< Pointer to the request semaphore.
        sem_t *rsp;     ///< Pointer to the response semaphore.
        char req_name[64]; ///< Name of the request semaphore (for reference and cleanup).
        char rsp_name[64]; ///< Name of the response semaphore (for reference and cleanup).
} gio_sem_t;

extern int gio_sem_open(gio_sem_t *s, const gio_sem_cfg_t *cfg);
extern int gio_sem_post_req(gio_sem_t *s);
extern int gio_sem_wait_rsp(gio_sem_t *s, uint32_t timeout_ms);
extern int gio_sem_close(gio_sem_t *s);

#endif /* __MGR_GIO_GIO_IPC_SEM_H__ */