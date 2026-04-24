#ifndef __MGR_GIO_GIO_IPC_SHM_H__
#define __MGR_GIO_GIO_IPC_SHM_H__

#include <stddef.h>
#include <stdint.h>

/// @brief Structure representing a request for GIO IPC communication. Contains a request ID and an argument for the request.
typedef struct gio_ipc_req_s {
        uint32_t req_id; ///< Request ID for identifying the IPC request. This field can be used by the sender to specify the type of request being made, and by the receiver to determine how to process the request. The specific meaning of different request IDs should be defined by the application using this IPC mechanism.
        int32_t arg; ///< Argument for the IPC request. This field can be used to pass additional data or parameters associated with the request. The specific interpretation of this argument should be defined by the application using this IPC mechanism, and may depend on the request ID or other context of the request.
} gio_ipc_req_t;

typedef struct gio_ipc_rsp_s {
        uint32_t req_id;        ///< Request ID for identifying the IPC response. This field can be used by the sender to specify the type of response being sent, and by the receiver to determine how to process the response. The specific meaning of different request IDs should be defined by the application using this IPC mechanism.
        int32_t rc;             ///< Return code for the IPC response. This field can be used to indicate the success or failure of the request, and may contain additional information about the result of the request. The specific interpretation of this return code should be defined by the application using this IPC mechanism.
        int32_t data;           ///< Data associated with the IPC response. This field can be used to pass additional data or results associated with the response. The specific interpretation of this data should be defined by the application using this IPC mechanism, and may depend on the request ID or other context of the response.   
} gio_ipc_rsp_t;

#define GIO_IPC_BLOCK_BYTES ((size_t)(sizeof(gio_ipc_req_t) + sizeof(gio_ipc_rsp_t)))

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