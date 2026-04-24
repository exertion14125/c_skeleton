#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "ra/gio/gio_ipc_shm.h"

/// @brief Internal structure representing a block of shared memory used for GIO IPC.
typedef struct gio_ipc_block_s {
        gio_ipc_req_t req; ///< Request structure for IPC communication. This field will be used by the sender to write the request data, and by the receiver to read the request data from shared memory.
        gio_ipc_rsp_t rsp; ///< Response structure for IPC communication. This field will be used by the sender to write the response data, and by the receiver to read the response data from shared memory.
} gio_ipc_block_t;

/// @brief Structure representing a shared memory segment used for GIO IPC communication. Contains the file descriptor, size, name, and memory pointer for the shared memory segment.
/// @param s Pointer to the gio_shm_t structure where the shared memory segment will be represented. This structure will be populated with the file descriptor, size, name, and memory pointer for the shared memory segment if the operation is successful.
/// @param name Name of the shared memory segment to be created or opened. This name will be used for creating and accessing the shared memory segment. It must be a null-terminated string.
/// @param size Size of the shared memory segment to be created or opened. This size will determine the amount of memory allocated for the shared memory segment. If the specified size is smaller than the size of the gio_ipc_block_t structure, it will be automatically adjusted to ensure that the shared memory segment can accommodate at least one block of IPC data.
/// @return 0 on success, or a negative error code on failure. The gio_shm_t structure pointed to by s will contain the file descriptor, size, name, and memory pointer for the shared memory segment if the operation is successful.
int gio_shm_open(gio_shm_t *s, const char *name, size_t size)
{
        if (!s || !name || name[0] == '\0') {
                return -EINVAL;
        }

        memset(s, 0, sizeof(*s));
        s->fd = shm_open(name, O_CREAT | O_RDWR, 0660);
        if (s->fd < 0) {
                return -errno;
        }

        if (size < sizeof(gio_ipc_block_t)) {
                size = sizeof(gio_ipc_block_t);
        }

        if (ftruncate(s->fd, (off_t)size) != 0) {
                int err = errno;
                close(s->fd);
                s->fd = -1;
                return -err;
        }

        s->size = size;
        strncpy(s->name, name, sizeof(s->name) - 1U);
        s->name[sizeof(s->name) - 1U] = '\0';
        return 0;
}

/// @brief Maps the shared memory segment represented by the gio_shm_t structure into the process's address space, allowing it to be accessed through the mem pointer in the structure.
/// @param s Pointer to the gio_shm_t structure representing the shared memory segment to be mapped. The fd and size fields of this structure must have been initialized by a successful call to gio_shm_open before calling this function.
/// @return 0 on success, or a negative error code on failure. The mem pointer in the gio_shm_t structure will be set to the base address of the mapped shared memory segment if the operation is successful.
int gio_shm_map(gio_shm_t *s)
{
        if (!s || s->fd < 0 || s->size == 0U) {
                return -EINVAL;
        }

        s->mem = mmap(NULL, s->size, PROT_READ | PROT_WRITE, MAP_SHARED, s->fd, 0);
        if (s->mem == MAP_FAILED) {
                s->mem = NULL;
                return -errno;
        }

        return 0;
}

/// @brief Writes a request structure to the shared memory segment represented by the gio_shm_t structure. This function should be called by the process that is producing requests to write a new request to shared memory before posting the request semaphore to notify the consumer process.
/// @param s Pointer to the gio_shm_t structure representing the shared memory segment where the request will be written. The mem pointer of this structure must have been initialized by a successful call to gio_shm_map before calling this function.
/// @param req Pointer to the gio_ipc_req_t structure containing the request data to be written to shared memory. This structure should be populated with the request ID and argument that will be sent to the consumer process.
/// @return 0 on success, or a negative error code on failure. The request data will be written to the shared memory segment if the operation is successful.
int gio_shm_write_req(gio_shm_t *s, const gio_ipc_req_t *req)
{
        gio_ipc_block_t *blk;
        if (!s || !s->mem || !req) {
                return -EINVAL;
        }

        blk = (gio_ipc_block_t *)s->mem;
        blk->req = *req;
        return 0;
}

/// @brief Reads a response structure from the shared memory segment represented by the gio_shm_t structure. This function should be called by the process that is consuming requests to read the response from shared memory after waiting on the response semaphore to be sign
/// @param s Pointer to the gio_shm_t structure representing the shared memory segment from which the response will be read. The mem pointer of this structure must have been initialized by a successful call to gio_shm_map before calling this function.
/// @param rsp Pointer to the gio_ipc_rsp_t structure where the read response data will be stored. This structure will be populated with the request ID, return code, and data from the response read from shared memory if the operation is successful.
/// @return 0 on success, or a negative error code on failure. The response data will be read from the shared memory segment and stored in the rsp structure if the operation is successful.
int gio_shm_read_rsp(gio_shm_t *s, gio_ipc_rsp_t *rsp)
{
        gio_ipc_block_t *blk;
        if (!s || !s->mem || !rsp) {
                return -EINVAL;
        }

        blk = (gio_ipc_block_t *)s->mem;
        *rsp = blk->rsp;
        return 0;
}

/// @brief Unmaps the shared memory segment represented by the gio_shm_t structure from the process's address space, allowing it to be released and cleaned up. This function should be called when the shared memory segment is no longer needed and should be unmapped before closing the shared memory segment with gio_shm_close.
/// @param s Pointer to the gio_shm_t structure representing the shared memory segment to be unmapped. The mem pointer of this structure must have been initialized by a successful call to gio_shm_map before calling this function.
/// @return 0 on success, or a negative error code on failure. The mem pointer in the gio_shm_t structure will be set to NULL if the operation is successful.
int gio_shm_unmap(gio_shm_t *s)
{
        if (!s) {
                return -EINVAL;
        }
        if (s->mem) {
                if (munmap(s->mem, s->size) != 0) {
                        return -errno;
                }
                s->mem = NULL;
        }
        return 0;
}

/// @brief Closes the shared memory segment represented by the gio_shm_t structure, unmaps it from the process's address space if it is currently mapped, and releases any resources associated with the shared memory segment. This function should be called when the shared memory segment is no longer needed and should be closed after unmapping it with gio_shm_unmap.
/// @param s Pointer to the gio_shm_t structure representing the shared memory segment to be closed. The fd field of this structure must have been initialized by a successful call to gio_shm_open before calling this function. The mem pointer should have been set to NULL by a successful call to gio_shm_unmap before calling this function.
/// @return 0 on success, or a negative error code on failure. The fd field in the gio_shm_t structure will be set to -1 and the mem pointer will be set to NULL if the operation is successful.
int gio_shm_close(gio_shm_t *s)
{
        if (!s) {
                return -EINVAL;
        }

        (void)gio_shm_unmap(s);
        if (s->fd >= 0) {
                (void)close(s->fd);
                s->fd = -1;
        }

        return 0;
}