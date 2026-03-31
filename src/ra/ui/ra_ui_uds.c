#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/socket.h>

#include "ra/ui/ra_ui_uds.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/// @brief RA UI UDS server configuration structure.
struct ra_ui_uds_cfg_s {
        char sock_path[107]; ///< UNIX domain socket path.
        int backlog; ///< Listen backlog.
        int chmod_mode; ///< Socket file chmod mode (e.g., 0660). 0 means no chmod.
        int nonblock; ///< Set non-blocking mode for sockets.
};
typedef struct ra_ui_uds_cfg_s ra_ui_uds_cfg_t;

/// @brief RA UI UDS server context structure.
struct ra_ui_uds_srv_s {
        ra_ui_uds_cfg_t cfg; ///< Configuration.
        int listen_fd; ///< Listening socket file descriptor.
        int client_fd; ///< Connected client socket file descriptor.
        // int opened; ///< Whether the server is opened.
};

/// @brief Set non-blocking mode on a file descriptor.
/// @param fd File descriptor.
/// @return 0 on success, negative errno on failure.
static int set_nonblock(int fd, bool nonblock)
{
        if (fd < 0) {
                return -EINVAL;
        }
        int fl = fcntl(fd, F_GETFL, 0);
        if (fl < 0) {
                return -errno;
        }
        if (nonblock) {
            fl |= O_NONBLOCK;
        } else {
            fl &= ~O_NONBLOCK;
        }
        if (fcntl(fd, F_SETFL, fl) < 0) {
                return -errno;
        }
        return 0;
}

/// @brief Create RA UI UDS server context.
/// @param sock_path UNIX domain socket path.
/// @param backlog Listen backlog.
/// @param chmod_mode Socket file chmod mode (e.g., 0660). 0 means no chmod.
/// @param nonblock Set non-blocking mode for sockets.
/// @return Pointer to RA UI UDS server context, or NULL on failure.
ra_ui_uds_srv_t *ra_ui_uds_srv_alloc(void)
{
        return (ra_ui_uds_srv_t*)calloc(1, sizeof(ra_ui_uds_srv_t));
}

/// @brief Destroy RA UI UDS server context.
/// @param srv Pointer to RA UI UDS server context pointer.
void ra_ui_uds_srv_destroy(ra_ui_uds_srv_t **srv)
{
        if (!srv || !*srv) {
                return;
        }
        ra_ui_uds_srv_deinit(*srv);
        free(*srv);
        *srv = NULL;
}

/// @brief Initialize RA UI UDS server context.
/// @param srv RA UI UDS server context.
/// @param sock_path UNIX domain socket path.
/// @param backlog Listen backlog.
/// @param chmod_mode Socket file chmod mode (e.g., 0660). 0 means no chmod.
/// @param nonblock Set non-blocking mode for sockets.
/// @return 0 on success, negative errno on failure.
int ra_ui_uds_srv_init(ra_ui_uds_srv_t *srv, const char *sock_path, int backlog, int chmod_mode, int nonblock)
{
        if (!srv || !sock_path || sock_path[0] == '\0') {
                return -EINVAL;
        }

        memset(srv, 0, sizeof(*srv));
        strncpy(srv->cfg.sock_path, sock_path, sizeof(srv->cfg.sock_path) - 1);
        srv->cfg.backlog = (backlog > 0) ? backlog : 4;
        srv->cfg.chmod_mode = (chmod_mode != 0) ? chmod_mode : 0660;;
        srv->cfg.nonblock = nonblock ? 1 : 0;

        srv->listen_fd = -1;
        srv->client_fd = -1;

        return 0;
}

/// @brief Deinitialize RA UI UDS server context.
/// @param srv RA UI UDS server context.
void ra_ui_uds_srv_deinit(ra_ui_uds_srv_t *srv)
{
        if (!srv) return;
        ra_ui_uds_srv_close(srv);
        memset(srv, 0, sizeof(*srv));
}

/// @brief Open RA UI UDS server.
/// @param srv RA UI UDS server context.
/// @return 0 on success, negative errno on failure.
int ra_ui_uds_srv_open(ra_ui_uds_srv_t *srv)
{
        if (!srv || !srv->cfg.sock_path || srv->cfg.sock_path[0] == '\0') {
                return -EINVAL;
        }
        if (srv->listen_fd >= 0) {
                return 0;
        }
        // if (srv->opened) {
        //         return 0;
        // }

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
                return -errno;
        }

        unlink(srv->cfg.sock_path);

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, srv->cfg.sock_path, sizeof(addr.sun_path) - 1);
        
        if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                int e = -errno;
                close(fd);
                return e;
        }

        if (srv->cfg.chmod_mode > 0) {
                if (chmod(srv->cfg.sock_path, (mode_t)srv->cfg.chmod_mode) < 0) {
                        int e = -errno;
                        close(fd);
                        unlink(srv->cfg.sock_path);
                        return e;
                }
        }

        if (listen(fd, srv->cfg.backlog) < 0) {
                int e = -errno;
                close(fd);
                unlink(srv->cfg.sock_path);
                return e;
        }

        if (srv->cfg.nonblock) {
                int rc = set_nonblock(fd, true);
                if (rc) {
                        close(fd);
                        unlink(srv->cfg.sock_path);
                        return rc;
                }
        }

        srv->listen_fd = fd;
        return 0;
}

/// @brief Close RA UI UDS server.
/// @param srv RA UI UDS server context.
void ra_ui_uds_srv_close(ra_ui_uds_srv_t *srv)
{
        if (!srv) {
                return;
        }
        ra_ui_uds_drop_client(srv);

        if (srv->listen_fd >= 0) {
                close(srv->listen_fd);
                srv->listen_fd = -1;
        }

        if (srv->cfg.sock_path) {
                unlink(srv->cfg.sock_path);
        }
}

/// @brief Get listening socket file descriptor.
/// @param srv RA UI UDS server context.
/// @return Listening socket file descriptor, or -1 if srv is NULL.
int ra_ui_uds_listen_fd(const ra_ui_uds_srv_t *srv)
{
        return srv ? srv->listen_fd : -1;
}

/// @brief Get connected client socket file descriptor.
/// @param srv RA UI UDS server context.
/// @return Connected client socket file descriptor, or -1 if srv is NULL.
int ra_ui_uds_client_fd(const ra_ui_uds_srv_t *srv)
{
        return srv ? srv->client_fd : -1;
}

/// @brief Set the connected client socket file descriptor.
/// @param srv RA UI UDS server context.
/// @param client_fd Connected client socket file descriptor.
/// @return 0 on success, negative errno on failure.
int ra_ui_uds_set_client(ra_ui_uds_srv_t *srv, int client_fd)
{
        if (!srv) {
                return -EINVAL;
        }
        if (client_fd < 0) {
                return -EINVAL;
        }
        if (srv->client_fd >= 0) {
                return -EBUSY;
        }
        if (srv->cfg.nonblock) {
                int rc = set_nonblock(client_fd, true);
                if (rc) {
                        close(client_fd);
                        return rc;
                }
        }
        srv->client_fd = client_fd;
        return 0;
}

/// @brief Drop the connected client socket.
/// @param srv RA UI UDS server context.
void ra_ui_uds_drop_client(ra_ui_uds_srv_t *srv)
{
        if (!srv) return;
        if (srv->client_fd >= 0) {
                close(srv->client_fd);
                srv->client_fd = -1;
        }
}

/// @brief Accept a new client connection.
/// @param srv RA UI UDS server context.
/// @return New client socket file descriptor, or negative errno on failure.
int ra_ui_uds_accept(ra_ui_uds_srv_t *srv)
{
        if (!srv || srv->listen_fd < 0) {
                return -EINVAL;
        }

        int cfd = accept(srv->listen_fd, NULL, NULL);
        if (cfd < 0) {
                return -errno; 
        }

        return cfd;
}

/// @brief Receive data from a socket.
/// @param fd Socket file descriptor.
/// @param buf Buffer to receive data.
/// @param len Length of the buffer.
/// @return Number of bytes received, or negative errno on failure.
ssize_t ra_ui_uds_recv(int fd, void *buf, size_t len)
{
        if (fd < 0 || !buf || len == 0) {
                return -EINVAL;
        }
        ssize_t n = recv(fd, buf, len, 0);
        if (n >= 0) {
                return n;
        }
        return -(ssize_t)errno;
}

/// @brief Send data to a socket.
/// @param fd Socket file descriptor.
/// @param buf Buffer containing data to send.
/// @param len Length of the buffer.
/// @return Number of bytes sent, or negative errno on failure.
ssize_t ra_ui_uds_send(int fd, const void *buf, size_t len)
{
        if (fd < 0 || !buf || len == 0) {
                return -EINVAL;
        }
        ssize_t n = send(fd, buf, len, 0);
        if (n >= 0) {
                return n;
        }
        return -(ssize_t)errno;
}
