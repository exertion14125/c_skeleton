#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <pthread.h>

#include "ra/ui/ra_ui_uds.h"
#include "mgr/ui/ui_proto.h"
#include "mgr/ui/ui_mgr_priv.h"
#include "mgr/ui/ui_mgr.h"

#define UI_HANDSHAKE_TIMEOUT_DEFAULT_MS (3000)
#define UI_PING_RETRY_BACKOFF_MS        (100)
#define UI_PONG_TIMEOUT_DEFAULT_MS      (5000)


/// @brief Get current time in milliseconds.
/// @return Current time in milliseconds.
static uint64_t now_ms(void)
{
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/// @brief Update last transaction time on RX.
/// @param mgr UI manager.
/// @param now Current time in milliseconds.
static void touch_rx(ui_mgr_t *mgr, uint64_t now)
{
        if (!mgr) return;
        mgr->last_rx_ms = now;
}

/// @brief Update last transaction time on TX.
/// @param mgr UI manager.
/// @param now Current time in milliseconds.
static void touch_tx(ui_mgr_t *mgr, uint64_t now)
{
        if (!mgr) return;
        mgr->last_tx_ms = now;
        mgr->await_pong = 0;
        mgr->pong_deadline_ms = 0;
}

/// @brief Compare two integers and return the minimum.
/// @param a value a.
/// @param b value b.
/// @return Minimum of a and b.
static int min_int(int a, int b)
{
        return (a < b) ? a : b;
}

/// @brief Get handshake timeout in ms.
/// @param mgr UI manager.
/// @return Handshake timeout in milliseconds.
/// @note If stale_timeout_ms is configured (>0), we reuse it as handshake timeout
///       to avoid adding new cfg fields. Otherwise use default.
static int handshake_timeout_ms(const ui_mgr_t *mgr)
{
        if (!mgr) {
                return UI_HANDSHAKE_TIMEOUT_DEFAULT_MS;
        }
        if (mgr->cfg.stale_timeout_ms > 0) {
                int t = mgr->cfg.stale_timeout_ms;
                return (t < UI_HANDSHAKE_TIMEOUT_DEFAULT_MS) ? t : UI_HANDSHAKE_TIMEOUT_DEFAULT_MS;
        }
        return UI_HANDSHAKE_TIMEOUT_DEFAULT_MS;
}

/// @brief Check if handshake has timed out.
/// @param mgr UI manager.
/// @param now Current time in milliseconds.
/// @return 1 if timed out, 0 otherwise.
static int handshake_timed_out(const ui_mgr_t *mgr, uint64_t now)
{
        if (!mgr) {
                return 0;
        }
        if (mgr->state != UI_ST_HANDSHAKE) {
                return 0;
        }
        if (mgr->attached_ms == 0) { /* handshake begin not recorded */
                return 0;
        }
        return ((now - mgr->attached_ms) > (uint64_t)handshake_timeout_ms(mgr)) ? 1 : 0;
}

/// @brief Clamps poll timeout to the next scheduled ping to prevent interval drift.
/// @param mgr UI manager.
/// @param timeout_ms Original timeout in milliseconds.
/// @param now Current time in milliseconds.
/// @return Clamped timeout in milliseconds.
/// @note If not connected or interval <= 0, original timeout is preserved.
/// @note If ping is overdue, timeout is set to 0 for immediate processing.
static int clamp_poll_timeout(ui_mgr_t *mgr, int timeout_ms, uint64_t now)
{
        if (!mgr) {
                return timeout_ms;
        }
        if (timeout_ms < 0) {
                timeout_ms = mgr->cfg.poll_timeout_ms;
        }

        if (mgr->state != UI_ST_CONNECTED) {
                return timeout_ms;
        }
        if (mgr->cfg.ping_interval_ms <= 0) {
                return timeout_ms;
        }
        
        if (mgr->last_ping_ms == 0) { /// last_ping_ms is "next due time"
                return 0; /// schedule immediate ping try
        }
        if (now >= mgr->last_ping_ms) {
                return 0; /// due now
        }
        uint64_t diff = mgr->last_ping_ms - now;
        int ms_until = (diff > (uint64_t)INT32_MAX) ? INT32_MAX : (int)diff;
        return min_int(timeout_ms, ms_until);
}

///===== internal functions =====///

/// @brief Check if the current UI connection is stale.
/// @param mgr UI manager.
/// @param now Current time in milliseconds.
/// @return 1 if stale, 0 otherwise.
static int is_stale(ui_mgr_t *mgr, uint64_t now)
{
        if (!mgr) {
                return 0;
        }
        if (mgr->cfg.stale_timeout_ms <= 0) {
                return 0;
        }
        if (mgr->last_rx_ms == 0) {
                return 0;
        }
        return ((now - mgr->last_rx_ms) > (uint64_t)mgr->cfg.stale_timeout_ms) ? 1 : 0;
}


/// @brief Reset the hello buffer and related state.
/// @param mgr UI manager.
static void hello_reset(ui_mgr_t *mgr)
{
        if (!mgr) {
                return;
        }
        mgr->hello_last_seen_wseq = 0;
}

/// @brief Handle incoming data from the current UI connection during handshake or normal operation.
/// @param mgr UI manager.
/// @param payload Pointer to the received payload data.
/// @param payload_len Length of the received payload data in bytes.
/// @return 0 on success, 1 if more data is needed to complete handshake, negative error code on failure.
static int handle_hello_frame(ui_mgr_t *mgr, const unsigned char *payload, size_t payload_len)
{
        ui_hello_payload_t hello;

        if (!mgr || !payload) {
                return -EINVAL;
        }
        if (payload_len != sizeof(ui_hello_payload_t)) {
                return -EPROTO;
        }

        memcpy(&hello, payload, sizeof(hello));
        mgr->hello_last_seen_wseq = hello.last_seen_wseq;
        return 0;
}

/// @brief Callback for UI detach event.
/// @param mgr UI manager.
/// @param fd File descriptor of the detached UI connection.
static void cb_detach(ui_mgr_t *mgr, int fd)
{
        if (!mgr) {
                return;
        }
        if (mgr->cb.on_detach) {
                mgr->cb.on_detach(mgr->cb.user, fd);
        }
}

/// @brief Callback for UI attach event.
/// @param mgr UI manager.
/// @param fd File descriptor of the attached UI connection.
/// @param last_seen_wseq Last seen wseq from UI hello message.
/// @return 0 on success, negative error code on failure.
static int cb_attach(ui_mgr_t *mgr, int fd, uint64_t last_seen_wseq)
{
        if (!mgr) {
                return -EINVAL;
        }
        if (!mgr->cb.on_attach) {
                return 0;
        }
        return mgr->cb.on_attach(mgr->cb.user, fd, last_seen_wseq);
}


/// @brief Drop the current UI connection.
/// @param mgr UI manager.
static void drop_ui(ui_mgr_t *mgr)
{
        if (!mgr) {
                return;
        }

        int cfd = ra_ui_uds_client_fd(mgr->srv);

        if (cfd >= 0) {
                cb_detach(mgr, cfd);
        }
        ra_ui_uds_drop_client(mgr->srv);

        mgr->last_rx_ms = 0;
        mgr->last_tx_ms = 0;
        mgr->last_ping_ms = 0; ///next due time reset
        mgr->attached_ms = 0;
        mgr->await_pong = 0;
        mgr->pong_deadline_ms = 0;

        hello_reset(mgr);

        mgr->state = UI_ST_IDLE;
}

/// @brief New UI connection attach and initialize handshake.
/// @param mgr UI manager.
/// @param now Current time in milliseconds.
/// @return 0 on success, 1 need more data, negative error code on failure.
static int ui_rx_handshake(ui_mgr_t *mgr, uint64_t now)
{
        int fd;
        unsigned char tmp[512];
        ssize_t n;
        ui_frame_hdr_t hdr;
        unsigned char payload[UI_FRAME_PAYLOAD_MAX];
        size_t payload_len;
        int rc;

        if (!mgr) {
                return -EINVAL;
        }

        fd = ra_ui_uds_client_fd(mgr->srv);
        if (fd < 0) {
                return -EINVAL;
        }

        for (;;) {
                n = ra_ui_uds_recv(fd, tmp, sizeof(tmp));
                if (n == 0) {
                        return -EPIPE;
                }
                if (n < 0) {
                        int e = (int)(-n);
                        if (e == EINTR) {
                                continue;
                        }
                        if (e == EAGAIN || e == EWOULDBLOCK) {
                                return 1; /* need more data */
                        }
                        return -e;
                }

                touch_rx(mgr, now);

                rc = ui_proto_append_rx(&mgr->rx_buf, tmp, (size_t)n);
                if (rc != 0) {
                        return rc;
                }

                for (;;) {
                        rc = ui_proto_try_parse(&mgr->rx_buf,
                                                &hdr,
                                                payload,
                                                sizeof(payload),
                                                &payload_len);
                        if (rc == 1) {
                                return 1; /* frame incomplete */
                        }
                        if (rc != 0) {
                                return rc;
                        }

                        if (hdr.type != UI_FRAME_HELLO) {
                                return -EPROTO;
                        }

                        rc = handle_hello_frame(mgr, payload, payload_len);
                        if (rc != 0) {
                                return rc;
                        }

                        return 0; /* hello frame complete */
                }
        }
}

/// @brief Attach new UI connection (begin handshake).
/// @param mgr UI manager.
/// @param cfd Client file descriptor.
/// @param now Current time in milliseconds.
static void attach_ui_begin(ui_mgr_t *mgr, int cfd, uint64_t now)
{
        (void)cfd;
        mgr->last_tx_ms = 0;
        mgr->attached_ms = now; ///handshake begin timestamp
        mgr->await_pong = 0;
        mgr->pong_deadline_ms = 0;

        hello_reset(mgr);

        mgr->state = UI_ST_HANDSHAKE;
}

/// @brief Finish UI connection attach after handshake.
/// @param mgr UI manager.
/// @param cfd Client file descriptor.
/// @param now Current time in milliseconds.
static void finish_attach(ui_mgr_t *mgr, int cfd, uint64_t now)
{
        int rc = cb_attach(mgr, cfd, mgr->hello_last_seen_wseq);
        if (rc != 0) { //server rejected attach => drop
                drop_ui(mgr);
                return;
        }
        mgr->attached_ms = now;
        touch_rx(mgr, now);
        mgr->last_ping_ms = 0; ///next ping due time reset
        mgr->await_pong = 0;
        mgr->pong_deadline_ms = 0;
        mgr->state = UI_ST_CONNECTED;
}

/// @brief Check peer credentials of the connected UI client against the configured policy.
/// @param mgr UI manager.
/// @param cfd Client file descriptor.
/// @return 0 on success, negative error code on failure.
static int check_peer_cred(ui_mgr_t *mgr, int cfd)
{
        if (!mgr) {
                return -EINVAL;
        }
        if (cfd < 0) {
                return -EINVAL;
        }

        if (!mgr->cfg.enable_peer_cred_check) {
                return 0;
        }

#if defined(SO_PEERCRED)
        struct ucred cred;
        socklen_t len = (socklen_t)sizeof(cred);
        memset(&cred, 0, sizeof(cred));
        if (getsockopt(cfd, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0) {
                return -errno;
        }
        /// Root UID (0) is a special case that can be allowed by allow_root_uid even if allow_uid is not 0, but still subject to peer credential checks if enabled.
        if (cred.uid == 0 && mgr->cfg.allow_root_uid) {
                return 0;
        }
        /// UID check (if allow_uid is set, it must match; if allow_uid is not set, any UID is allowed)
        if (mgr->cfg.allow_uid >= 0) {
                if ((int)cred.uid != mgr->cfg.allow_uid) {
                        // fprintf(stderr, "ui_mgr: reject peer by uid pid=%ld uid=%ld gid=%ld\n", (long)cred.pid, (long)cred.uid, (long)cred.gid);
                        return -EACCES;
                }
        }
        /// GID check (if allow_gid is set, it must match; if allow_gid is not set, any GID is allowed)
        if (mgr->cfg.allow_gid >= 0) {
                if ((int)cred.gid != mgr->cfg.allow_gid) {
                        // fprintf(stderr, "ui_mgr: reject peer by gid pid=%ld uid=%ld gid=%ld\n", (long)cred.pid, (long)cred.uid, (long)cred.gid);
                        return -EACCES;
                }
        }
        return 0;
#else
        (void)cfd;
        return -ENOTSUP;
#endif
}

/// @brief Handle new UI connection acceptance based on policy.
/// @param mgr UI manager.
/// @param now Current time in milliseconds.
static void handle_accept(ui_mgr_t *mgr, uint64_t now)
{
        if (!mgr) {
                return;
        }

        int cfd = ra_ui_uds_accept(mgr->srv); /* accept only, does NOT set client_fd */
        if (cfd < 0) {
                if (cfd == -EAGAIN || cfd == -EWOULDBLOCK) {
                        return;
                }
                /* other accept error: ignore */
                return;
        }
        
        int vrc = check_peer_cred(mgr, cfd);
        if (vrc != 0) {
                close(cfd);
                return;
        }

        int has = (ra_ui_uds_client_fd(mgr->srv) >= 0);

        /* no existing client => set & begin handshake */
        if (!has) {
                int rc = ra_ui_uds_set_client(mgr->srv, cfd);
                if (rc != 0) {
                        /* unexpected: someone set client in between */
                        close(cfd);
                        return;
                }
                attach_ui_begin(mgr, cfd, now);
                return;
        }

        /* has existing client */
        if (mgr->cfg.limit_policy == UI_LIMIT_REJECT) {
                /* reject new connection */
                close(cfd);
                return;
        }

        /* replace policy */
        if (mgr->cfg.replace_mode == UI_REPLACE_IF_STALE) {
                if (!is_stale(mgr, now)) {
                        /* not stale => keep old, reject new */
                        close(cfd);
                        return;
                }
        }

        /* replace: drop old first (must clear srv->client_fd to satisfy -EBUSY contract) */
        drop_ui(mgr);

        /* now should be no client */
        int rc = ra_ui_uds_set_client(mgr->srv, cfd);
        if (rc != 0) {
                /* if still -EBUSY something is wrong; close new fd */
                close(cfd);
                return;
        }

        attach_ui_begin(mgr, cfd, now);
}

/// @brief Handle incoming data from the current UI connection.
/// @param mgr UI manager.
/// @param now Current time in milliseconds.
/// @note Protocol: 1-byte commands 
static void handle_ui_rx(ui_mgr_t *mgr, uint64_t now)
{
        int rc;
        int fd;
        unsigned char tmp[512];
        
        ssize_t n;
        ui_frame_hdr_t hdr;
        unsigned char payload[UI_FRAME_PAYLOAD_MAX];
        size_t payload_len;

        if (!mgr) {
                return;
        }

        fd = ra_ui_uds_client_fd(mgr->srv);
        if (fd < 0) {
                return;
        }

        while(1) {
                n = ra_ui_uds_recv(fd, tmp, sizeof(tmp));
                if (n == 0) {
                        drop_ui(mgr);
                        return;
                }
                if (n < 0) {
                        int e = (int)(-n);
                        if (e == EINTR) {
                                continue;
                        }
                        if (e == EAGAIN || e == EWOULDBLOCK) {
                                break;
                        }
                        drop_ui(mgr);
                        return;
                }

                touch_rx(mgr, now);

                rc = ui_proto_append_rx(&mgr->rx_buf, tmp, (size_t)n);
                if (rc != 0) {
                        drop_ui(mgr);
                        return;
                }

                while(1) {
                        rc = ui_proto_try_parse(&mgr->rx_buf,
                                                &hdr,
                                                payload,
                                                sizeof(payload),
                                                &payload_len);
                        if (rc == 1) {
                                break;
                        }
                        if (rc != 0) {
                                drop_ui(mgr);
                                return;
                        }

                        switch (hdr.type) {
                        case UI_FRAME_HELLO:
                                if (mgr->state == UI_ST_HANDSHAKE) {
                                        if (handle_hello_frame(mgr, payload, payload_len) == 0) {
                                                finish_attach(mgr, fd, now);
                                        } else {
                                                drop_ui(mgr);
                                                return;
                                        }
                                }
                                break;

                        case UI_FRAME_PONG:
                                mgr->await_pong = 0;
                                mgr->pong_deadline_ms = 0;
                                break;

                        default:
                                /* 1차 패치에서는 unknown frame 무시 또는 로깅 */
                                break;
                        }
                }
        }
}

/// @brief Check if a ping is due to be sent.
/// @param mgr UI manager.
/// @param now Current time in milliseconds.
/// @return 1 if ping is due, 0 otherwise.
static int ping_due(const ui_mgr_t *mgr, uint64_t now)
{
        if (!mgr) {
                return 0;
        }
        if (mgr->cfg.ping_interval_ms <= 0) {
                return 0;
        }
        if (mgr->state != UI_ST_CONNECTED) {
                return 0;
        }

        if (mgr->last_ping_ms == 0) {
                return 1; ///immediate first ping
        }
        return (now >= mgr->last_ping_ms) ? 1 : 0;
}

/// @brief Send a ping to the UI client.
/// @param mgr UI manager.
/// @param now Current time in milliseconds.
/// @return 0 on success, 1 need retry later, negative error code on failure.
static int ping_send(ui_mgr_t *mgr, uint64_t now)
{
        int fd;
        int rc;

        if (!mgr) {
                return -EINVAL;
        }
        fd = ra_ui_uds_client_fd(mgr->srv);
        if (fd < 0) {
                return -EINVAL;
        }

        rc = ui_proto_send_frame(fd, UI_FRAME_PING, 0, NULL, 0);
        if (rc == 0) {
                touch_tx(mgr, now);
                mgr->last_ping_ms = now + (uint64_t)mgr->cfg.ping_interval_ms;
                mgr->await_pong = 1;
                mgr->pong_deadline_ms = now + (uint64_t)UI_PONG_TIMEOUT_DEFAULT_MS;
                return 0;
        }
        if (rc == -EAGAIN || rc == -EWOULDBLOCK || rc == -EINTR) {
                mgr->last_ping_ms = now + (uint64_t)UI_PING_RETRY_BACKOFF_MS;
                mgr->await_pong = 1;
                mgr->pong_deadline_ms = now + (uint64_t)UI_PONG_TIMEOUT_DEFAULT_MS;
                return 1; /* retry later */
        }
        return rc; /* other error */
}


/// @brief Maybe send a ping to the UI client.
/// @param mgr UI manager.
/// @param now Current time in milliseconds.
static void maybe_ping(ui_mgr_t *mgr, uint64_t now)
{
        if (!ping_due(mgr, now)) {
                return;
        }
        int rc = ping_send(mgr, now);
        if (rc < 0) {
                drop_ui(mgr);
        }
}

/// @brief Handle stale UI connection.
/// @param mgr UI manager.
/// @param now Current time in milliseconds.
static void handle_stale(ui_mgr_t *mgr, uint64_t now)
{
        if (!mgr) {
                return;
        }
        if (ra_ui_uds_client_fd(mgr->srv) < 0) {
                return;
        }
        if (mgr->await_pong && mgr->pong_deadline_ms != 0 && now >= mgr->pong_deadline_ms) {
                drop_ui(mgr);
                return;
        }
        if (handshake_timed_out(mgr, now)) {
                drop_ui(mgr);
                return;
        }
        if (mgr->cfg.stale_timeout_ms <= 0) {
                return;
        }

        if (is_stale(mgr, now)) {
                drop_ui(mgr);
        }
}

///===== external API =====///

/// @brief Allocate UI manager.
/// @return Allocated UI manager, or NULL on failure.
ui_mgr_t *alloc_ui_mgr(void)
{
        return (ui_mgr_t*)calloc(1, sizeof(ui_mgr_t));
}

/// @brief Destroy UI manager.
/// @param mgr Pointer to UI manager pointer.
void destroy_ui_mgr(ui_mgr_t **mgr)
{
        if (!mgr || !*mgr) {
                return;
        }
        deinit_ui_mgr(*mgr);
        free(*mgr);
        *mgr = NULL;
}


/// @brief Initialize UI manager.
/// @param mgr UI manager.
/// @param cfg UI manager configuration.
/// @param sink_ui log_sink_ui adapter (lifecycle managed externally).
/// @return 0 on success, negative error code on failure.
int init_ui_mgr(ui_mgr_t *mgr, const ui_mgr_cfg_t *cfg, const ui_mgr_cb_t *cb)
{
        if (!mgr || !cfg) {
                return -EINVAL;
        }
        if (cfg->sock_path[0] == '\0') {
                return -EINVAL;
        }

        memset(mgr, 0, sizeof(ui_mgr_t));
        mgr->cfg = *cfg;

        if (cb) {
                mgr->cb = *cb;
        } else {
                memset(&mgr->cb, 0, sizeof(mgr->cb));
        }

        if (mgr->cfg.backlog <= 0) {
                mgr->cfg.backlog = 4;
        }
        if (mgr->cfg.chmod_mode == 0) {
                mgr->cfg.chmod_mode = 0660;
        }
        if (mgr->cfg.enable_peer_cred_check) {
                if (mgr->cfg.allow_uid == 0 && mgr->cfg.allow_root_uid == 0) {
                        mgr->cfg.allow_uid = -1;
                }
                if (mgr->cfg.allow_gid == 0) {
                        mgr->cfg.allow_gid = -1;
                }
        }
        mgr->srv = ra_ui_uds_srv_alloc();
        if (!mgr->srv) {
                return -ENOMEM;
        }
        int rc = ra_ui_uds_srv_init(mgr->srv, mgr->cfg.sock_path, mgr->cfg.backlog, mgr->cfg.chmod_mode, mgr->cfg.nonblock);
        if (rc) {
                ra_ui_uds_srv_destroy(&mgr->srv);
                return rc;
        }
        ui_rx_buf_init(&mgr->rx_buf);
        mgr->state = UI_ST_INIT;
        return 0;
}

/// @brief Deinitialize UI manager.
/// @param mgr UI manager.
void deinit_ui_mgr(ui_mgr_t *mgr)
{
        if (!mgr) {
                return;
        }
        stop_ui_mgr(mgr);
        ra_ui_uds_srv_destroy(&mgr->srv);
        memset(mgr, 0, sizeof(*mgr));
}

/// @brief Start UI manager (begin listening for connections).
/// @param mgr UI manager.
/// @return 0 on success, negative error code on failure.
int start_ui_mgr(ui_mgr_t *mgr)
{
        if (!mgr) {
                return -EINVAL;
        }
        int rc = ra_ui_uds_srv_open(mgr->srv);
        if (rc) {
                return rc;
        }
        mgr->state = UI_ST_IDLE;
        return 0;
}

/// @brief Stop UI manager (close connections and listening socket).
/// @param mgr UI manager.
void stop_ui_mgr(ui_mgr_t *mgr)
{
        if (!mgr) {
                return;
        }
        drop_ui(mgr);
        ra_ui_uds_srv_close(mgr->srv);
        mgr->state = UI_ST_SHUTDOWN;
}

/// @brief Get current state of UI manager.
/// @param mgr UI manager.
/// @return Current state of UI manager.
ui_mgr_state_t get_ui_mgr_state(const ui_mgr_t *mgr)
{
        return mgr ? mgr->state : UI_ST_SHUTDOWN;
}

/// @brief Poll UI manager once (non-blocking).
/// @param mgr UI manager.
/// @param timeout_ms Poll timeout in milliseconds.
/// @return 0 on success, negative error code on failure.
int ui_mgr_poll_once(ui_mgr_t *mgr, int timeout_ms)
{
        if (!mgr || (mgr->state == UI_ST_SHUTDOWN)) {
                return -EINVAL;
        }
        uint64_t now = now_ms();
        int lfd = ra_ui_uds_listen_fd(mgr->srv);
        int cfd = ra_ui_uds_client_fd(mgr->srv);

        struct pollfd pfds[2];
        int nfds = 0;

        int idx_lfd = -1;
        int idx_cfd = -1;

        if (lfd >= 0) {
                idx_lfd = nfds;
                pfds[nfds].fd = lfd;
                pfds[nfds].events = POLLIN;
                pfds[nfds].revents = 0;
                nfds++;
        }
        if (cfd >= 0) {
                idx_cfd = nfds;
                pfds[nfds].fd = cfd;
                pfds[nfds].events = POLLIN | POLLHUP | POLLERR;
                pfds[nfds].revents = 0;
                nfds++;
        }

        timeout_ms = clamp_poll_timeout(mgr, timeout_ms, now);

        int pr = poll(pfds, nfds, timeout_ms);
        if (pr < 0) {
                if (errno == EINTR) {
                        return 0;
                }
                return -errno;
        }
        if (pr > 0) {
                // Handle accept event
                if (idx_lfd >= 0 && (pfds[idx_lfd].revents & POLLIN)) {
                        // printf("UI mgr: new connection incoming\n");
                        handle_accept(mgr, now);
                }
                // Handle UI RX event
                cfd = ra_ui_uds_client_fd(mgr->srv);
                if (cfd >= 0 && idx_cfd >= 0 && pfds[idx_cfd].fd == cfd) {
                        if (pfds[idx_cfd].revents & (POLLHUP | POLLERR)) {
                                drop_ui(mgr);
                        } else if (pfds[idx_cfd].revents & POLLIN) {
                                if (mgr->state == UI_ST_HANDSHAKE) {
                                        int hr = ui_rx_handshake(mgr, now);
                                        if (hr == 0) {
                                                finish_attach(mgr, cfd, now);
                                        } else if (hr < 0) {
                                                drop_ui(mgr);
                                        }
                                } else if (mgr->state == UI_ST_CONNECTED) {
                                        handle_ui_rx(mgr, now);
                                }
                        }
                }
        }

        now = now_ms();
        maybe_ping(mgr, now);
        handle_stale(mgr, now);
        return 0;
}

/// @brief Notify the UI that a snapshot is ready.
/// @param mgr UI manager.
/// @param kind Snapshot kind (defined by UI_MSG_NOTIFY_SNAPSHOT_READY protocol).
/// @param seq Snapshot sequence number (defined by UI_MSG_NOTIFY_SNAPSHOT_READY protocol).
/// @return 0 on success, negative error code on failure.
int ui_mgr_notify_snapshot_ready(ui_mgr_t *mgr, uint16_t kind, uint32_t seq)
{
        int fd;
        int rc;
        ui_notify_snapshot_payload_t payload;

        if (!mgr) {
                return -EINVAL;
        }
        if (mgr->state != UI_ST_CONNECTED) {
                return -ENOTCONN;
        }

        fd = ra_ui_uds_client_fd(mgr->srv);
        if (fd < 0) {
                return -ENOTCONN;
        }

        memset(&payload, 0, sizeof(payload));
        payload.kind = kind;
        payload.seq = seq;

        rc = ui_proto_send_frame(fd, UI_FRAME_NOTIFY_SNAPSHOT, 0, &payload, sizeof(payload));
        if (rc != 0) {
                return rc;
        }

        touch_tx(mgr, now_ms());
        return 0;
}

/// @brief Send a log chunk to the UI.
/// @param mgr UI manager.
/// @param seq Log chunk sequence number (defined by UI_MSG_LOG_CHUNK protocol).
/// @param level Log level (defined by UI_MSG_LOG_CHUNK protocol).
/// @param text Log text (null-terminated string, truncated if exceeds UI_LOG_TEXT_MAX).
/// @return 0 on success, negative error code on failure.
int ui_mgr_send_log_chunk(ui_mgr_t *mgr, uint32_t seq, uint16_t level, const char *text)
{
        ui_log_chunk_payload_t payload;
        int fd;
        size_t text_len;
        int rc;

        if (!mgr || !text) {
                return -EINVAL;
        }

        if (mgr->state != UI_ST_CONNECTED) {
                return -ENOTCONN;
        }

        fd = ra_ui_uds_client_fd(mgr->srv);
        if (fd < 0) {
                return -ENOTCONN;
        }

        memset(&payload, 0, sizeof(payload));
        payload.seq = seq;
        payload.level = level;

        text_len = strlen(text);
        if (text_len >= UI_LOG_TEXT_MAX) {
                text_len = UI_LOG_TEXT_MAX - 1;
        }

        payload.text_len = (uint16_t)text_len;
        memcpy(payload.text, text, text_len);
        payload.text[text_len] = '\0';

        rc = ui_proto_send_frame(fd,
                                 UI_FRAME_LOG_CHUNK,
                                 0,
                                 &payload,
                                 sizeof(payload));
        if (rc != 0) {
                return rc;
        }

        touch_tx(mgr, now_ms());
        return 0;
}