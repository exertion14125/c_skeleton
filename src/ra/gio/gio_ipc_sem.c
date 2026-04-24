#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include "ra/gio/gio_ipc_sem.h"

/// @brief Configuration structure for initializing a GIO IPC semaphore pair.
/// @param s Pointer to the gio_sem_t structure where the opened semaphores will be stored. This structure will be populated with the semaphore pointers and names if the operation is successful.
/// @param cfg Pointer to the configuration structure containing the names and initial values for the request and response semaphores. The req_name and rsp_name fields must be null-terminated strings, and the req_init and rsp_init fields specify the initial values for the semaphores.
/// @return 0 on success, or a negative error code on failure. The gio_sem_t structure pointed to by s will contain the opened semaphores and their names if the operation is successful.
int gio_sem_open(gio_sem_t *s, const gio_sem_cfg_t *cfg)
{
        unsigned int req_init;
        unsigned int rsp_init;

        if (!s || !cfg || !cfg->req_name || !cfg->rsp_name) {
                return -EINVAL;
        }

        memset(s, 0, sizeof(*s));
        req_init = cfg->req_init;
        rsp_init = cfg->rsp_init;

        s->req = sem_open(cfg->req_name, O_CREAT, 0660, req_init);
        if (s->req == SEM_FAILED) {
                s->req = NULL;
                return -errno;
        }

        s->rsp = sem_open(cfg->rsp_name, O_CREAT, 0660, rsp_init);
        if (s->rsp == SEM_FAILED) {
                int err = errno;
                sem_close(s->req);
                s->req = NULL;
                s->rsp = NULL;
                return -err;
        }

        strncpy(s->req_name, cfg->req_name, sizeof(s->req_name) - 1U);
        strncpy(s->rsp_name, cfg->rsp_name, sizeof(s->rsp_name) - 1U);
        s->req_name[sizeof(s->req_name) - 1U] = '\0';
        s->rsp_name[sizeof(s->rsp_name) - 1U] = '\0';
        return 0;
}

/// @brief Posts (signals) the request semaphore to indicate that a new request is available. This function should be called by the process that is producing requests after it has written a request to shared memory and wants to notify the consumer process that a new request is ready for processing.
/// @param s Pointer to the gio_sem_t structure containing the request semaphore to be posted. The req field of this structure must have been initialized by a successful call to gio_sem_open.
/// @return 0 on success, or a negative error code on failure. The request semaphore will be posted if the operation is successful.
int gio_sem_post_req(gio_sem_t *s)
{
        if (!s || !s->req) {
                return -EINVAL;
        }

        if (sem_post(s->req) != 0) {
                return -errno;
        }

        return 0;
}

/// @brief Waits on the response semaphore with an optional timeout. This function should be called by the process that is consuming requests after it has processed a request and wants to wait for the producer process to acknowledge the response by posting the response semaphore. If timeout_ms is 0, this function will wait indefinitely until the response semaphore is posted. If timeout_ms is greater than 0, this function will wait for the specified number of milliseconds for the response semaphore to be posted, and will return if the timeout expires before the semaphore is posted.
/// @param s Pointer to the gio_sem_t structure containing the response semaphore to wait on
/// @param timeout_ms Timeout in milliseconds to wait for the response semaphore to be posted. If this value is 0, the function will wait indefinitely until the semaphore is posted. If this value is greater than 0, the function will wait for the specified number of milliseconds before timing out.
/// @return 0 on success (response semaphore was posted), or a negative error code on failure (e.g., -EINVAL for invalid parameters, -ETIMEDOUT for timeout expiration). The function will return 0 if the response semaphore is successfully waited on, or a negative error code if an error occurs or if the timeout expires before the semaphore is posted.
int gio_sem_wait_rsp(gio_sem_t *s, uint32_t timeout_ms)
{
        struct timespec ts;

        if (!s || !s->rsp) {
                return -EINVAL;
        }

        if (timeout_ms == 0U) {
                if (sem_wait(s->rsp) != 0) {
                        return -errno;
                }
                return 0;
        }
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += (time_t)(timeout_ms / 1000U);
        ts.tv_nsec += (long)((timeout_ms % 1000U) * 1000000UL);
        if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000L;
        }
        if (sem_timedwait(s->rsp, &ts) != 0) {
                if (errno == ETIMEDOUT) {
                        return -ETIMEDOUT;
                }
                return -errno;
        }
        return 0;
}

/// @brief Closes the semaphores in the gio_sem_t structure and releases any resources associated with them. This function should be called by both the producer and consumer processes when they are done using the semaphores to clean up resources. After calling this function, the req and rsp fields of the gio_sem_t structure will be set to NULL, and the names of the semaphores will remain in the req_name and rsp_name fields for reference but will not be used for any operations.
/// @param s Pointer to the gio_sem_t structure containing the semaphores to be closed. The req and rsp fields of this structure should have been initialized by a successful call to gio_sem_open. After this function is called, the req and rsp fields will be set to NULL, and the names of the semaphores will remain in the req_name and rsp_name fields for reference but will not be used for any operations.
/// @return 0 on success, or a negative error code on failure. The semaphores in the gio_sem_t structure will be closed and their pointers set to NULL if the operation is successful.
int gio_sem_close(gio_sem_t *s)
{
        if (!s) {
                return -EINVAL;
        }

        if (s->req) {
                sem_close(s->req);
                s->req = NULL;
        }
        if (s->rsp) {
                sem_close(s->rsp);
                s->rsp = NULL;
        }
        return 0;
}