#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>

#include "time.h"

#include "include/skeleton_version.h"

#include "util/log/log_bootstrap.h"

#include "mgr/cfg/cfg_mgr.h"
#include "mgr/gio/gio_mgr.h"
#include "mgr/red/red_mgr.h"
#include "mgr/logic/logic_mgr.h"
#include "mgr/sys/sys_mgr.h"
#include "mgr/ui/ui_mgr_bootstrap.h"
#include "mgr/ui/ui_snapshot_mgr.h"
#include "ui/ui_snapshot_shm.h"

#define UI_SNAP_KIND_MAIN           0

static ui_snapshot_mgr_t* ui_snapshot_mgr = NULL;
static ui_mgr_t* ui_mgr = NULL;
static cfg_mgr_t* cfg_mgr = NULL;
static gio_mgr_t* gio_mgr = NULL;
static red_mgr_t* red_mgr = NULL;
static logic_mgr_t* logic_mgr = NULL;
static sys_mgr_t* sys_mgr = NULL;
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

//===== snapshot publish sample. =====//
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

//===== Manager bus sample. =====//
#include "mgr/contract/mgr_addrs.h"
#include "util/mgr_bus/mgr_bus.h"
static const mgr_bus_addr_t g_mgr_addrs[] = {
        APP_MGR_ADDR_SYS,       /// MAIN Manager. Main FSM and Process Control.
        APP_MGR_ADDR_CFG,       /// Configuration Manager. Configuration file read/write and management.
        APP_MGR_ADDR_GIO,       /// General I/O Manager. RS-485/CAN BUS I/O control and management.
        APP_MGR_ADDR_RED,       /// Redundancy Manager. Redundancy control and management for high availability.
        APP_MGR_ADDR_LOGIC,     /// Logic Manager. User logic control and management.
        APP_MGR_ADDR_UI         /// Monitoring UI Manager. User interface management.
};

/// @brief Create a sample manager bus with predefined addresses and queue capacity.
/// @return Pointer to the created manager bus instance, or NULL on failure.
mgr_bus_t *create_sample_mgr_bus(void)
{
        mgr_bus_cfg_t cfg = {
                .addrs = g_mgr_addrs,
                .addr_count = sizeof(g_mgr_addrs) / sizeof(g_mgr_addrs[0]),
                .qcap = 64
        };
        return mgr_bus_create(&cfg);
}


/// @brief Main function. Common argument pointer declare.
/// @param argc argument count.
/// @param argv argument value.
int main(int argc, char *argv[]) 
{
        mgr_bus_t *mgr_bus = NULL;

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

        //===== Create manager bus first.
        mgr_bus = create_sample_mgr_bus();
        if (mgr_bus == NULL) {
                LOGE_T("MAIN", "Failed to create sample manager bus.\n");
                goto skeleton_exit;
        }

        //===== Bootstrap domain managers.
        cfg_mgr = bootstrap_cfg_mgr();
        if (!cfg_mgr) {
                LOGE_T("MAIN", "Failed to bootstrap CFG manager.\n");
                goto skeleton_exit;
        }

        gio_mgr = bootstrap_gio_mgr();
        if (!gio_mgr) {
                LOGE_T("MAIN", "Failed to bootstrap GIO manager.\n");
                goto skeleton_exit;
        }

        red_mgr = bootstrap_red_mgr();
        if (!red_mgr) {
                LOGE_T("MAIN", "Failed to bootstrap RED manager.\n");
                goto skeleton_exit;
        }

        logic_mgr = bootstrap_logic_mgr();
        if (!logic_mgr) {
                LOGE_T("MAIN", "Failed to bootstrap LOGIC manager.\n");
                goto skeleton_exit;
        }
        sys_mgr = bootstrap_sys_mgr();
        if (!sys_mgr) {
                LOGE_T("MAIN", "Failed to bootstrap SYS manager.\n");
                goto skeleton_exit;
        }
        //==== ui log init
        ui_mgr = bootstrap_ui_mgr();
        if (ui_mgr == NULL) {
                LOGE_T("MAIN", "Failed to bootstrap UI manager.\n");
                goto skeleton_exit;
        } else {
                g_ui_log_init = true;
        }

        //===== Bind bus to managers.
        if (cfg_mgr_bind_bus(cfg_mgr, mgr_bus) != 0) {
                LOGE_T("MAIN", "Failed to bind CFG manager bus.\n");
                goto skeleton_exit;
        }
        if (gio_mgr_bind_bus(gio_mgr, mgr_bus) != 0) {
                LOGE_T("MAIN", "Failed to bind GIO manager bus.\n");
                goto skeleton_exit;
        }
        if (red_mgr_bind_bus(red_mgr, mgr_bus) != 0) {
                LOGE_T("MAIN", "Failed to bind RED manager bus.\n");
                goto skeleton_exit;
        }
        if (logic_mgr_bind_bus(logic_mgr, mgr_bus) != 0) {
                LOGE_T("MAIN", "Failed to bind LOGIC manager bus.\n");
                goto skeleton_exit;
        }
        if (sys_mgr_bind_bus(sys_mgr, mgr_bus) != 0) {
                LOGE_T("MAIN", "Failed to bind SYS manager bus.\n");
                goto skeleton_exit;
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

        //===== Start runloops: CFG -> GIO -> RED -> UI -> SYS
        if (cfg_mgr_start_runloop(cfg_mgr) != 0) {
                LOGE_T("MAIN", "Failed to start CFG manager runloop.\n");
                goto skeleton_exit;
        }
        if (gio_mgr_start_runloop(gio_mgr) != 0) {
                LOGE_T("MAIN", "Failed to start GIO manager runloop.\n");
                goto skeleton_exit;
        }
        if (red_mgr_start_runloop(red_mgr) != 0) {
                LOGE_T("MAIN", "Failed to start RED manager runloop.\n");
                goto skeleton_exit;
        }
        if (logic_mgr_start_runloop(logic_mgr) != 0) {
                LOGE_T("MAIN", "Failed to start LOGIC manager runloop.\n");
                goto skeleton_exit;
        }
        if (sys_mgr_start_runloop(sys_mgr) != 0) {
                LOGE_T("MAIN", "Failed to start SYS manager runloop.\n");
                goto skeleton_exit;
        }
        if (ui_mgr_start_runloop(ui_mgr) != 0) {
                LOGE_T("MAIN", "Failed to start UI manager runloop.\n");
                goto skeleton_exit;
        }
        //===== Request start: CFG -> GIO -> RED -> UI -> SYS
        if (cfg_mgr_request_start(cfg_mgr) != 0) {
                LOGE_T("MAIN", "Failed to request CFG manager start.\n");
                goto skeleton_exit;
        }
        if (gio_mgr_request_start(gio_mgr) != 0) {
                LOGE_T("MAIN", "Failed to request GIO manager start.\n");
                goto skeleton_exit;
        }
        if (red_mgr_request_start(red_mgr) != 0) {
                LOGE_T("MAIN", "Failed to request RED manager start.\n");
                goto skeleton_exit;
        }
        if (logic_mgr_request_start(logic_mgr) != 0) {
                LOGE_T("MAIN", "Failed to request LOGIC manager start.\n");
                goto skeleton_exit;
        }
        if (sys_mgr_request_start(sys_mgr) != 0) {
                LOGE_T("MAIN", "Failed to request SYS manager start.\n");
                goto skeleton_exit;
        }
        if (ui_mgr_request_start(ui_mgr) != 0) {
                LOGE_T("MAIN", "Failed to request UI manager start.\n");
                goto skeleton_exit;
        }

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
        if (mgr_bus) {
                mgr_bus_wakeup(mgr_bus);
        }
        if (ui_mgr) {
                (void)ui_mgr_stop_runloop(ui_mgr);
        }
        if (sys_mgr) {
                (void)sys_mgr_stop_runloop(sys_mgr);
        }
        if (logic_mgr) {
                (void)logic_mgr_stop_runloop(logic_mgr);
        }
        if (red_mgr) {
                (void)red_mgr_stop_runloop(red_mgr);
        }
        if (gio_mgr) {
                (void)gio_mgr_stop_runloop(gio_mgr);
        }
        if (cfg_mgr) {
                (void)cfg_mgr_stop_runloop(cfg_mgr);
        }

        if (ui_snapshot_mgr) {
                destroy_ui_snapshot_mgr(&ui_snapshot_mgr);
                ui_snapshot_mgr = NULL;
        }
        if (ui_mgr) {
                destroy_ui_mgr(&ui_mgr);
                ui_mgr = NULL;
        }
        if (sys_mgr) {
                destroy_sys_mgr(&sys_mgr);
                sys_mgr = NULL;
        }
        if (logic_mgr) {
                destroy_logic_mgr(&logic_mgr);
                logic_mgr = NULL;
        }
        if (red_mgr) {
                destroy_red_mgr(&red_mgr);
                red_mgr = NULL;
        }
        if (gio_mgr) {
                destroy_gio_mgr(&gio_mgr);
                gio_mgr = NULL;
        }
        if (cfg_mgr) {
                destroy_cfg_mgr(&cfg_mgr);
                cfg_mgr = NULL;
        }
        if (mgr_bus) {
                mgr_bus_destroy(&mgr_bus);
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