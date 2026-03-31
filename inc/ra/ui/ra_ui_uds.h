#ifndef __RA_UI_UDS_H__
#define __RA_UI_UDS_H__

#include <stddef.h>
#include <stdint.h>

typedef struct ra_ui_uds_srv_s ra_ui_uds_srv_t;

extern ra_ui_uds_srv_t *ra_ui_uds_srv_alloc(void);
extern void ra_ui_uds_srv_destroy(ra_ui_uds_srv_t **srv);

extern int  ra_ui_uds_srv_init(ra_ui_uds_srv_t *srv, const char *sock_path, int backlog, int chmod_mode, int nonblock);
extern void ra_ui_uds_srv_deinit(ra_ui_uds_srv_t *srv);

extern int  ra_ui_uds_srv_open(ra_ui_uds_srv_t *srv);
extern void ra_ui_uds_srv_close(ra_ui_uds_srv_t *srv);

extern int  ra_ui_uds_listen_fd(const ra_ui_uds_srv_t *srv);
extern int  ra_ui_uds_client_fd(const ra_ui_uds_srv_t *srv);

extern int  ra_ui_uds_set_client(ra_ui_uds_srv_t *srv, int client_fd);
extern void ra_ui_uds_drop_client(ra_ui_uds_srv_t *srv);
extern int  ra_ui_uds_accept(ra_ui_uds_srv_t *srv);

extern ssize_t ra_ui_uds_recv(int fd, void *buf, size_t len); /* returns n, or -errno */
extern ssize_t ra_ui_uds_send(int fd, const void *buf, size_t len); /* returns n, or -errno */

#endif /* __RA_UI_UDS_H__ */