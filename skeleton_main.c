#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>

#include "time.h"

#include "include/skeleton_version.h"

#include "util/log/log_bootstrap.h"

#include "mgr/ui/ui_mgr_bootstrap.h"
#include "mgr/ui/ui_snapshot_mgr.h"
#include "ui/ui_snapshot_shm.h"

#define UI_SNAP_KIND_MAIN           0

static ui_snapshot_mgr_t* ui_snapshot_mgr = NULL;
static ui_mgr_t* ui_mgr = NULL;
static volatile bool g_ui_log_init = 0;
static volatile bool g_init = 0;
static volatile bool g_run = 0;

//===== signal handle function. =====//

/// @brief User signal handle function. For using kill -10/-11 pid command. 
///       Process can be monitored by an external program. Process can be used for simulation control.
/// @param signo signal number.
/// @param info signal information.
/// @param ctx kernel context.
void proc_user_mon_signal(int signo, siginfo_t *info, void *uctx)
{
        (void)uctx; // Explicitly mark uctx as unused.
        if (g_ui_log_init == false || g_init == false || g_run == false) {
                return;
        }
        if (signo != SIGUSR1 /* && signo != SIGUSR2 */) {
                return;
        }
        if (info == NULL) {
                return;
        }
        // int opt = info->si_value.sival_int;
        (void)info;
        // Process user signal.
        LOGI_T("MAIN", "Received user monitor signal: %d\r\n", signo);
        (void)ui_mgr_request_start(ui_mgr);
}

/// @brief User signal handle registor function.
/// @param void
void user_mon_signal_handling(void)
{
        static struct sigaction act_user;
        act_user.sa_flags	= SA_SIGINFO;
        act_user.sa_sigaction   = proc_user_mon_signal;
        sigaction(SIGUSR1, &act_user, NULL);
        // sigaction(SIGUSR2, &act_user, NULL);
}

/// @brief Stop siganl handle registor function.  
/// @param exit_func signal handle function.
static void signal_register_stop(void (*exit_func)(int))
{
        signal(SIGTERM, exit_func);     // Termination signal
        signal(SIGINT , exit_func);     // Terminal interrupt (CTRL+b)
        signal(SIGQUIT, exit_func);     // Terminal interrupt (CTRL+c)
        signal(SIGPIPE, SIG_IGN);	// Pipe write Error. (Broken pipe: write to pipe with no readers).
        signal(SIGHUP , SIG_IGN);       // Terminal interface broken (cannot be caught or ignored).
        // signal(SIGSTOP, NULL);       // Process stop/pause (cannot be caught or ignored).
        // signal(SIGTSTP, NULL);       // CTRL+Z (Normal Stop Process. -> SIGCONT. Release).
        // signal(SIGCONT, NULL);       // Process SIGTSTP/SIGSTOP re-start (cannot be caught or ignored).
}

/// @brief Exit application.
/// @param signal input signal.
static void stop_app(int signal)
{
        printf("\r\n");
        printf("Exit App Signal[%d] \r\n", signal);
        g_run = false;
}

/* ============================================================
 * snapshot publish sample
 * ============================================================ */
static uint64_t get_now_ms(void)
{
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}

static void publish_sample_ui_snapshot(void)
{
        static uint64_t last_pub_ms = 0;
        static uint32_t heartbeat = 0;

        ui_snapshot_payload_t payload;
        uint32_t seq = 0;
        uint64_t now_ms;

        if (!ui_snapshot_mgr) {
                printf("UI snapshot manager not initialized, cannot publish snapshot.\n");
                return;
        }

        now_ms = get_now_ms();
        if ((now_ms - last_pub_ms) < 1000ULL) {
                return;
        }
        last_pub_ms = now_ms;

        memset(&payload, 0, sizeof(payload));

        payload.page_id = 1;
        payload.conn_state = (ui_mgr != NULL) ? (uint32_t)get_ui_mgr_state(ui_mgr) : 0;
        payload.alarm_count = heartbeat % 10U;

        snprintf(payload.title, sizeof(payload.title), "SKELETON MAIN");
        snprintf(payload.status_line, sizeof(payload.status_line),
                 "RUNNING hb=%u now=%llu",
                 heartbeat,
                 (unsigned long long)now_ms);

        heartbeat++;

        if (publish_ui_snapshot(ui_snapshot_mgr, &payload, &seq) == 0) {
                if (ui_mgr) {
                        (void)ui_mgr_notify_snapshot_ready(ui_mgr,
                                                           UI_SNAP_KIND_MAIN,
                                                           seq);
                }
        } else {
                printf("Failed to publish UI snapshot.\n");
                LOGE_T("MAIN", "publish_ui_snapshot failed\n");
        }
}

/// @brief Main function. Common argument pointer declare.
/// @param argc argument count.
/// @param argv argumnet value.
int main(int argc, char *argv[]) 
{
        (void)argc;
        (void)argv;
        printf("\033[2J\033[H"); // Clear screen and move cursor to home position.
        printf("\n\r");
        printf("----------------------------\n\r");
        printf("  SKELETON : %s\n\r", __DATE__);
        printf("----------------------------\n\r");
        printf("\n\r");

        printf("%s\r\n", SKELETON_SW_ENG_VERSION);

        //===== Initialize log system.
        if (bootstrap_log_system() != 0) {
                fprintf(stderr, "Failed to bootstrap log system.\n");
                return -1;
        }
        //==== ui log init
        ui_mgr = bootstrap_ui_mgr();
        if (ui_mgr == NULL) {
                LOGE_T("MAIN", "Failed to bootstrap UI manager.\n");
        } else {
                g_ui_log_init = true;
        }
        if (ui_mgr) {
                if (ui_mgr_start_runloop(ui_mgr) != 0) {
                        LOGE_T("MAIN", "Failed to start ui_mgr runloop.\n");
                        destroy_ui_mgr(&ui_mgr);
                        ui_mgr = NULL;
                        goto skeleton_exit;
                }
        }
        //==== Initialize UI snapshot manager.
        ui_snapshot_mgr = alloc_ui_snapshot_mgr();
        if (ui_snapshot_mgr == NULL) {
                LOGE_T("MAIN", "Failed to alloc UI snapshot manager.\n");
                goto skeleton_exit;
        }
        if (init_ui_snapshot_mgr(ui_snapshot_mgr, "/" APP_EXEC_NAME "_ui_snapshot") != 0) {
                LOGE_T("MAIN", "Failed to init UI snapshot manager.\n");
                destroy_ui_snapshot_mgr(&ui_snapshot_mgr);
                goto skeleton_exit;
        }
        //===== Initialize skeleton process. 
        // skeleton_proc_t* skeleton_proc = bootstrap_skeleton_proc();
        // if (skeleton_proc == NULL) {
        //         LOGE_T("SKELETON", "Failed to bootstrap SKELETON process.\n");
        //         goto skeleton_exit;
        // }
        g_init = true;

        // Signal register.
        signal_register_stop(stop_app);
        // User signal register for process monitoring and control.
        user_mon_signal_handling();
        g_run = true;
        // ======
        while (g_run) {
                // usleep(1); // Sleep 1 usec.;
                publish_sample_ui_snapshot();

                usleep(100 * 1000);
        }
skeleton_exit:
        if (ui_snapshot_mgr) {
                destroy_ui_snapshot_mgr(&ui_snapshot_mgr);
                ui_snapshot_mgr = NULL;
        }
        if (ui_mgr) {
                (void)ui_mgr_stop_runloop(ui_mgr);
                 destroy_ui_mgr(&ui_mgr);
                 ui_mgr = NULL;
        }
        exit_log_system();
        printf("\n\r");
        printf("----------------------------\n\r");
        printf("  SKELETON : EXIT\n\r");
        printf("----------------------------\n\r");
        printf("\n\r");
        return 0;
}

/// @mainpage
/// @section    Intro
/// - Skeleton application.
/// @section    Developer
/// - KIM JEONGGI
/// @section    Program
/// - Program Name : skeleton
///
/// @section    History
/// -# 2026-03-31 Ver01. Start coding.