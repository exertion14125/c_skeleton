/*
 * example/ui/ncui_ui_main.c
 *
 * 목적:
 *   - skeleton 프로세스에 SIGUSR1 전송하여 UI manager 시작 요청
 *   - 기존 UI log UDS 경로 유지
 *   - hello("ULOG" + last_seen_wseq LE64) 전송
 *   - 서버 ping('P')에 pong('K') 응답
 *   - snapshot은 SHM에서 읽고, UDS notify로 갱신 트리거만 받음
 *   - ncurses 로 log/raw 상태 + snapshot 상태 표시
 *
 * 실행 예:
 *   1) skeleton 실행
 *   2) ex_ncui_ui <skeleton_pid>
 *
 * 키:
 *   q : 종료
 *   r : reconnect
 *   h : hello 재전송
 *   s : SIGUSR1 재전송
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <ncurses.h>

#ifndef MON_EXEC_NAME
#define MON_EXEC_NAME "skeleton"
#endif

#ifndef DEF_LOG_UI_UDS_PATH
#define DEF_LOG_UI_UDS_PATH "/var/run/"
#endif

#define UI_HELLO_MAGIC0 'U'
#define UI_HELLO_MAGIC1 'L'
#define UI_HELLO_MAGIC2 'O'
#define UI_HELLO_MAGIC3 'G'

#define UI_PING_BYTE 'P'
#define UI_PONG_BYTE 'K'

#define UI_HELLO_SIZE               12
#define UI_DEFAULT_POLL_TIMEOUT_MS  100
#define UI_RECONNECT_DELAY_MS       1000
#define UI_RX_BUF_SIZE              512

#define UI_SNAPSHOT_MAGIC           0x55495348u
#define UI_SNAPSHOT_VERSION         1u

#define UI_MSG_NOP                  0
#define UI_MSG_NOTIFY_SNAPSHOT_READY 1

#define UI_SNAP_KIND_MAIN           0

static volatile sig_atomic_t g_run = 1;

/* ============================================================
 * snapshot shm layout
 * ============================================================ */

typedef struct ui_snapshot_hdr_s {
        uint32_t magic;
        uint16_t version;
        uint16_t hdr_size;
        uint32_t total_size;

        uint32_t active_idx;
        uint32_t seq;
        uint32_t flags;
        uint32_t reserved;
} ui_snapshot_hdr_t;

typedef struct ui_snapshot_payload_s {
        uint32_t page_id;
        uint32_t conn_state;
        uint32_t alarm_count;
        uint32_t reserved0;

        char title[64];
        char status_line[128];
} ui_snapshot_payload_t;

typedef struct ui_snapshot_shm_layout_s {
        ui_snapshot_hdr_t hdr;
        ui_snapshot_payload_t buf[2];
} ui_snapshot_shm_layout_t;

/* ============================================================
 * notify proto
 * ============================================================ */

typedef struct ui_notify_msg_s {
        uint16_t type;
        uint16_t kind;
        uint32_t seq;
        uint32_t arg0;
        uint32_t arg1;
} ui_notify_msg_t;

/* ============================================================
 * local shm reader
 * ============================================================ */

typedef struct local_ui_shm_s {
        int fd;
        size_t size;
        char name[64];
        ui_snapshot_shm_layout_t *ptr;
} local_ui_shm_t;

/* ============================================================
 * app context
 * ============================================================ */

typedef struct app_ctx_s {
        pid_t skeleton_pid;

        int fd;
        int connected;

        int connect_try_count;
        int signal_try_count;

        int poll_timeout_ms;
        int reconnect_delay_ms;

        int ping_rx_count;
        int pong_tx_count;

        int rx_count;
        int tx_count;

        int log_rx_count;
        int notify_rx_count;

        int last_errno;
        int last_signal_rc;

        uint32_t last_snapshot_seq;
        uint64_t last_seen_wseq;
        uint64_t last_rx_ms;
        uint64_t last_tx_ms;

        char sock_path[108];
        char shm_name[64];
        char last_event[160];

        char last_log_line[160];

        local_ui_shm_t snap_shm;
        int snap_shm_opened;
        ui_snapshot_payload_t snap;
} app_ctx_t;

/* ============================================================
 * util
 * ============================================================ */

static void stop_app(int signo)
{
        (void)signo;
        g_run = 0;
}

static uint64_t now_ms(void)
{
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}

static void write_u64_le(unsigned char *p, uint64_t v)
{
        p[0] = (unsigned char)((v >> 0) & 0xFF);
        p[1] = (unsigned char)((v >> 8) & 0xFF);
        p[2] = (unsigned char)((v >> 16) & 0xFF);
        p[3] = (unsigned char)((v >> 24) & 0xFF);
        p[4] = (unsigned char)((v >> 32) & 0xFF);
        p[5] = (unsigned char)((v >> 40) & 0xFF);
        p[6] = (unsigned char)((v >> 48) & 0xFF);
        p[7] = (unsigned char)((v >> 56) & 0xFF);
}

static int set_nonblock(int fd)
{
        int fl;

        if (fd < 0) {
                return -1;
        }

        fl = fcntl(fd, F_GETFL, 0);
        if (fl < 0) {
                return -1;
        }

        if (fcntl(fd, F_SETFL, fl | O_NONBLOCK) < 0) {
                return -1;
        }

        return 0;
}

/* ============================================================
 * shm reader
 * ============================================================ */

static void close_snapshot_reader(app_ctx_t *ctx)
{
        if (!ctx) {
                return;
        }

        if (ctx->snap_shm.ptr && ctx->snap_shm.ptr != MAP_FAILED) {
                munmap((void *)ctx->snap_shm.ptr, ctx->snap_shm.size);
                ctx->snap_shm.ptr = NULL;
        }

        if (ctx->snap_shm.fd >= 0) {
                close(ctx->snap_shm.fd);
                ctx->snap_shm.fd = -1;
        }

        memset(&ctx->snap_shm, 0, sizeof(ctx->snap_shm));
        ctx->snap_shm.fd = -1;
        ctx->snap_shm_opened = 0;
    }

static int open_snapshot_reader(app_ctx_t *ctx)
{
        local_ui_shm_t *shm;

        if (!ctx) {
                return -1;
        }

        if (ctx->snap_shm_opened) {
                return 0;
        }

        shm = &ctx->snap_shm;
        memset(shm, 0, sizeof(*shm));
        shm->fd = -1;
        shm->size = sizeof(ui_snapshot_shm_layout_t);
        strncpy(shm->name, ctx->shm_name, sizeof(shm->name) - 1);

        shm->fd = shm_open(shm->name, O_RDWR, 0660);
        if (shm->fd < 0) {
                ctx->last_errno = errno;
                snprintf(ctx->last_event, sizeof(ctx->last_event),
                         "snapshot shm open fail errno=%d", errno);
                return -1;
        }

        shm->ptr = (ui_snapshot_shm_layout_t *)mmap(NULL,
                                                    shm->size,
                                                    PROT_READ | PROT_WRITE,
                                                    MAP_SHARED,
                                                    shm->fd,
                                                    0);
        if (shm->ptr == MAP_FAILED) {
                ctx->last_errno = errno;
                close(shm->fd);
                shm->fd = -1;
                shm->ptr = NULL;
                snprintf(ctx->last_event, sizeof(ctx->last_event),
                         "snapshot shm mmap fail errno=%d", errno);
                return -1;
        }

        ctx->snap_shm_opened = 1;
        snprintf(ctx->last_event, sizeof(ctx->last_event), "snapshot shm opened");
        return 0;
}

static int update_snapshot_from_shm(app_ctx_t *ctx)
{
        ui_snapshot_shm_layout_t *p;
        uint32_t seq1, seq2, idx;

        if (!ctx || !ctx->snap_shm_opened || !ctx->snap_shm.ptr) {
                return -1;
        }

        p = ctx->snap_shm.ptr;

        if (p->hdr.magic != UI_SNAPSHOT_MAGIC) {
                snprintf(ctx->last_event, sizeof(ctx->last_event),
                         "snapshot magic mismatch");
                return -1;
        }

        if (p->hdr.version != UI_SNAPSHOT_VERSION) {
                snprintf(ctx->last_event, sizeof(ctx->last_event),
                         "snapshot version mismatch");
                return -1;
        }

        do {
                seq1 = p->hdr.seq;
                idx = p->hdr.active_idx;
                memcpy(&ctx->snap, &p->buf[idx], sizeof(ctx->snap));
                seq2 = p->hdr.seq;
        } while (seq1 != seq2);

        ctx->last_snapshot_seq = seq2;
        snprintf(ctx->last_event, sizeof(ctx->last_event),
                 "snapshot updated seq=%u", seq2);
        return 0;
}

/* ============================================================
 * uds client
 * ============================================================ */

static void close_conn(app_ctx_t *ctx)
{
        if (!ctx) {
                return;
        }

        if (ctx->fd >= 0) {
                close(ctx->fd);
                ctx->fd = -1;
        }

        ctx->connected = 0;
}

static int trigger_ui_mgr_start(app_ctx_t *ctx)
{
        if (!ctx) {
                return -1;
        }

        if (ctx->skeleton_pid <= 0) {
                ctx->last_signal_rc = -1;
                snprintf(ctx->last_event, sizeof(ctx->last_event),
                         "invalid skeleton pid");
                return -1;
        }

        ctx->signal_try_count++;

        if (kill(ctx->skeleton_pid, SIGUSR1) != 0) {
                ctx->last_errno = errno;
                ctx->last_signal_rc = -1;
                snprintf(ctx->last_event, sizeof(ctx->last_event),
                         "SIGUSR1 send fail: pid=%ld errno=%d",
                         (long)ctx->skeleton_pid, errno);
                return -1;
        }

        ctx->last_signal_rc = 0;
        snprintf(ctx->last_event, sizeof(ctx->last_event),
                 "SIGUSR1 sent to pid=%ld", (long)ctx->skeleton_pid);
        return 0;
}

static int connect_server(app_ctx_t *ctx)
{
        int fd;
        struct sockaddr_un addr;

        if (!ctx) {
                return -1;
        }

        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
                ctx->last_errno = errno;
                snprintf(ctx->last_event, sizeof(ctx->last_event),
                         "socket fail: errno=%d", errno);
                return -1;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, ctx->sock_path, sizeof(addr.sun_path) - 1);

        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                ctx->last_errno = errno;
                snprintf(ctx->last_event, sizeof(ctx->last_event),
                         "connect fail: errno=%d", errno);
                close(fd);
                return -1;
        }

        if (set_nonblock(fd) != 0) {
                ctx->last_errno = errno;
                snprintf(ctx->last_event, sizeof(ctx->last_event),
                         "nonblock fail: errno=%d", errno);
                close(fd);
                return -1;
        }

        ctx->fd = fd;
        ctx->connected = 1;
        ctx->connect_try_count++;

        snprintf(ctx->last_event, sizeof(ctx->last_event),
                 "connected to ui socket");
        return 0;
}

static int send_hello(app_ctx_t *ctx)
{
        unsigned char hello[UI_HELLO_SIZE];
        ssize_t n;

        if (!ctx || ctx->fd < 0) {
                return -1;
        }

        memset(hello, 0, sizeof(hello));
        hello[0] = UI_HELLO_MAGIC0;
        hello[1] = UI_HELLO_MAGIC1;
        hello[2] = UI_HELLO_MAGIC2;
        hello[3] = UI_HELLO_MAGIC3;
        write_u64_le(&hello[4], ctx->last_seen_wseq);

        n = send(ctx->fd, hello, sizeof(hello), 0);
        if (n != (ssize_t)sizeof(hello)) {
                ctx->last_errno = (n < 0) ? errno : EPIPE;
                snprintf(ctx->last_event, sizeof(ctx->last_event),
                         "hello send fail");
                return -1;
        }

        ctx->tx_count += (int)n;
        ctx->last_tx_ms = now_ms();

        snprintf(ctx->last_event, sizeof(ctx->last_event),
                 "hello sent (wseq=%llu)",
                 (unsigned long long)ctx->last_seen_wseq);
        return 0;
}

static int send_pong(app_ctx_t *ctx)
{
        char b = UI_PONG_BYTE;
        ssize_t n;

        if (!ctx || ctx->fd < 0) {
                return -1;
        }

        n = send(ctx->fd, &b, 1, 0);
        if (n != 1) {
                ctx->last_errno = (n < 0) ? errno : EPIPE;
                snprintf(ctx->last_event, sizeof(ctx->last_event),
                         "pong send fail");
                return -1;
        }

        ctx->pong_tx_count++;
        ctx->tx_count++;
        ctx->last_tx_ms = now_ms();

        snprintf(ctx->last_event, sizeof(ctx->last_event), "pong sent");
        return 0;
}

/* ============================================================
 * rx processing
 * ============================================================ */

static void process_notify_or_log(app_ctx_t *ctx, const unsigned char *buf, ssize_t n)
{
        if (!ctx || !buf || n <= 0) {
                return;
        }

        if (n == 1 && buf[0] == (unsigned char)UI_PING_BYTE) {
                ctx->ping_rx_count++;
                if (send_pong(ctx) != 0) {
                        close_conn(ctx);
                        return;
                }
                snprintf(ctx->last_event, sizeof(ctx->last_event), "ping received");
                return;
        }

        if (n == (ssize_t)sizeof(ui_notify_msg_t)) {
                const ui_notify_msg_t *msg = (const ui_notify_msg_t *)buf;

                if (msg->type == UI_MSG_NOTIFY_SNAPSHOT_READY) {
                        ctx->notify_rx_count++;
                        (void)update_snapshot_from_shm(ctx);
                        snprintf(ctx->last_event, sizeof(ctx->last_event),
                                 "snapshot notify seq=%u kind=%u",
                                 (unsigned)msg->seq,
                                 (unsigned)msg->kind);
                        return;
                }
        }

        /* 기존 log/raw stream은 그대로 유지: 여기서는 마지막 일부만 화면에 반영 */
        ctx->log_rx_count++;

        {
                size_t copy_len = (size_t)n;
                if (copy_len >= sizeof(ctx->last_log_line)) {
                        copy_len = sizeof(ctx->last_log_line) - 1;
                }

                memcpy(ctx->last_log_line, buf, copy_len);
                ctx->last_log_line[copy_len] = '\0';

                /* 화면 표시용으로 제어문자 일부 정리 */
                {
                        size_t i;
                        for (i = 0; i < copy_len; ++i) {
                                if ((unsigned char)ctx->last_log_line[i] < 0x20 &&
                                    ctx->last_log_line[i] != '\t' &&
                                    ctx->last_log_line[i] != ' ') {
                                        ctx->last_log_line[i] = '.';
                                }
                        }
                }
        }

        snprintf(ctx->last_event, sizeof(ctx->last_event),
                 "rx %ld bytes (log/raw)", (long)n);
}

static void process_rx(app_ctx_t *ctx)
{
        unsigned char buf[UI_RX_BUF_SIZE];
        ssize_t n;

        if (!ctx || ctx->fd < 0) {
                return;
        }

        n = recv(ctx->fd, buf, sizeof(buf), MSG_DONTWAIT);
        if (n == 0) {
                snprintf(ctx->last_event, sizeof(ctx->last_event),
                         "server closed connection");
                close_conn(ctx);
                return;
        }

        if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                        return;
                }

                ctx->last_errno = errno;
                snprintf(ctx->last_event, sizeof(ctx->last_event),
                         "recv fail errno=%d", errno);
                close_conn(ctx);
                return;
        }

        ctx->rx_count += (int)n;
        ctx->last_rx_ms = now_ms();

        process_notify_or_log(ctx, buf, n);
}

/* ============================================================
 * ui drawing
 * ============================================================ */

static void draw_ui(const app_ctx_t *ctx)
{
        int row = 0;

        erase();

        mvprintw(row++, 2, "SKELETON NCURSES UI EXAMPLE (LOG + SNAPSHOT)");
        mvprintw(row++, 2, "q:quit  r:reconnect  h:hello  s:send SIGUSR1");
        row++;

        mvprintw(row++, 2, "skeleton pid    : %ld", (long)ctx->skeleton_pid);
        mvprintw(row++, 2, "socket path     : %s", ctx->sock_path);
        mvprintw(row++, 2, "snapshot shm    : %s", ctx->shm_name);
        mvprintw(row++, 2, "connected       : %s", ctx->connected ? "YES" : "NO");
        mvprintw(row++, 2, "fd              : %d", ctx->fd);
        mvprintw(row++, 2, "signal tries    : %d", ctx->signal_try_count);
        mvprintw(row++, 2, "connect tries   : %d", ctx->connect_try_count);
        mvprintw(row++, 2, "signal rc       : %d", ctx->last_signal_rc);
        mvprintw(row++, 2, "last errno      : %d", ctx->last_errno);
        row++;

        mvprintw(row++, 2, "rx bytes        : %d", ctx->rx_count);
        mvprintw(row++, 2, "tx bytes        : %d", ctx->tx_count);
        mvprintw(row++, 2, "log rx count    : %d", ctx->log_rx_count);
        mvprintw(row++, 2, "notify rx count : %d", ctx->notify_rx_count);
        mvprintw(row++, 2, "ping rx count   : %d", ctx->ping_rx_count);
        mvprintw(row++, 2, "pong tx count   : %d", ctx->pong_tx_count);
        mvprintw(row++, 2, "last seen wseq  : %llu",
                 (unsigned long long)ctx->last_seen_wseq);
        mvprintw(row++, 2, "last rx ms      : %llu",
                 (unsigned long long)ctx->last_rx_ms);
        mvprintw(row++, 2, "last tx ms      : %llu",
                 (unsigned long long)ctx->last_tx_ms);
        row++;

        mvprintw(row++, 2, "snapshot seq    : %u", ctx->last_snapshot_seq);
        mvprintw(row++, 2, "snapshot title  : %s", ctx->snap.title);
        mvprintw(row++, 2, "status line     : %s", ctx->snap.status_line);
        mvprintw(row++, 2, "page id         : %u", ctx->snap.page_id);
        mvprintw(row++, 2, "conn state      : %u", ctx->snap.conn_state);
        mvprintw(row++, 2, "alarm count     : %u", ctx->snap.alarm_count);
        row++;

        mvprintw(row++, 2, "last event:");
        mvprintw(row++, 4, "%s", ctx->last_event);
        row++;

        mvprintw(row++, 2, "last log/raw rx:");
        mvprintw(row++, 4, "%s", ctx->last_log_line);
        row++;

        mvprintw(row++, 2, "usage:");
        mvprintw(row++, 4, "ex_ncui_ui <skeleton_pid>");
        mvprintw(row++, 4, "Maintain existing UI log stream + display snapshot shm");
        mvprintw(row++, 4, "Reread snapshot when notify is received");
        refresh();
}

/* ============================================================
 * keyboard
 * ============================================================ */

static void handle_key(app_ctx_t *ctx, int ch)
{
        if (!ctx) {
                return;
        }

        switch (ch) {
        case 'q':
        case 'Q':
                g_run = 0;
                break;

        case 'r':
        case 'R':
                close_conn(ctx);
                if (connect_server(ctx) == 0) {
                        if (send_hello(ctx) != 0) {
                                close_conn(ctx);
                        }
                }
                break;

        case 'h':
        case 'H':
                if (ctx->connected) {
                        if (send_hello(ctx) != 0) {
                                close_conn(ctx);
                        }
                }
                break;

        case 's':
        case 'S':
                (void)trigger_ui_mgr_start(ctx);
                break;

        default:
                break;
        }
}

/* ============================================================
 * main
 * ============================================================ */

int main(int argc, char *argv[])
{
        app_ctx_t ctx;
        struct pollfd pfd;
        int ch;
        int pr;

        memset(&ctx, 0, sizeof(ctx));
        ctx.fd = -1;
        ctx.snap_shm.fd = -1;
        ctx.poll_timeout_ms = UI_DEFAULT_POLL_TIMEOUT_MS;
        ctx.reconnect_delay_ms = UI_RECONNECT_DELAY_MS;
        ctx.last_signal_rc = -1;

        snprintf(ctx.sock_path, sizeof(ctx.sock_path),
                 "%s%s_ui.sock", DEF_LOG_UI_UDS_PATH, MON_EXEC_NAME);
        snprintf(ctx.shm_name, sizeof(ctx.shm_name),
                 "/%s_ui_snapshot", MON_EXEC_NAME);
        snprintf(ctx.last_event, sizeof(ctx.last_event), "init");
        snprintf(ctx.last_log_line, sizeof(ctx.last_log_line), "(none)");

        if (argc < 2) {
                fprintf(stderr, "usage: %s <skeleton_pid>\n", argv[0]);
                return 1;
        }

        ctx.skeleton_pid = (pid_t)atoi(argv[1]);
        if (ctx.skeleton_pid <= 0) {
                fprintf(stderr, "invalid skeleton pid: %s\n", argv[1]);
                return 1;
        }

        signal(SIGINT, stop_app);
        signal(SIGTERM, stop_app);

        initscr();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        nodelay(stdscr, TRUE);
        curs_set(0);

        (void)trigger_ui_mgr_start(&ctx);
        (void)open_snapshot_reader(&ctx);
        (void)update_snapshot_from_shm(&ctx);

        if (connect_server(&ctx) == 0) {
                if (send_hello(&ctx) != 0) {
                        close_conn(&ctx);
                }
        }

        while (g_run) {
                ch = getch();
                if (ch != ERR) {
                        handle_key(&ctx, ch);
                }

                if (!ctx.snap_shm_opened) {
                        (void)open_snapshot_reader(&ctx);
                }

                if (!ctx.connected) {
                        draw_ui(&ctx);
                        napms(ctx.reconnect_delay_ms);

                        if (connect_server(&ctx) == 0) {
                                if (send_hello(&ctx) != 0) {
                                        close_conn(&ctx);
                                }
                        }
                        continue;
                }

                pfd.fd = ctx.fd;
                pfd.events = POLLIN | POLLHUP | POLLERR;
                pfd.revents = 0;

                pr = poll(&pfd, 1, ctx.poll_timeout_ms);
                if (pr > 0) {
                        if (pfd.revents & (POLLHUP | POLLERR)) {
                                snprintf(ctx.last_event, sizeof(ctx.last_event),
                                         "poll hup/err");
                                close_conn(&ctx);
                        } else if (pfd.revents & POLLIN) {
                                process_rx(&ctx);
                        }
                } else if (pr < 0) {
                        if (errno != EINTR) {
                                ctx.last_errno = errno;
                                snprintf(ctx.last_event, sizeof(ctx.last_event),
                                         "poll fail: errno=%d", errno);
                                close_conn(&ctx);
                        }
                }

                draw_ui(&ctx);
        }

        close_conn(&ctx);
        close_snapshot_reader(&ctx);
        endwin();
        return 0;
}