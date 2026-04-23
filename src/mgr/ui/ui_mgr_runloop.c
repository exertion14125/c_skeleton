#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "mgr/ui/ui_mgr_priv.h"
#include "mgr/ui/ui_mgr.h"
#include "util/log/log_util.h"

#define UI_THREAD_STACK_SIZE         (64 * 1024) //64Kbyte

#define UI_RUNLOOP_IDLE_SLEEP_US   (100 * 1000)
#define UI_RUNLOOP_ERR_SLEEP_US    (100 * 1000)

static void *ui_mgr_runloop_thread_main(void *arg)
{
        ui_mgr_t *mgr = (ui_mgr_t *)arg;
        int rc;
        int poll_tm;

        if (!mgr) {
                return NULL;
        }

        while (mgr->runloop_run) {
                if (mgr->start_req && !mgr->started) {
                        mgr->start_req = 0U;

                        rc = start_ui_mgr(mgr);
                        if (rc == 0) {
                                mgr->started = 1U;
                                LOGI_T("UI_MGR", "ui_mgr runloop started UI manager.\n");
                        } else {
                                LOGE_T("UI_MGR", "start_ui_mgr failed rc=%d\n", rc);
                                usleep(UI_RUNLOOP_ERR_SLEEP_US);
                                // continue;
                        }
                }

                if (mgr->started) {
                        poll_tm = (mgr->cfg.poll_timeout_ms > 0) ?
                                  mgr->cfg.poll_timeout_ms : 100;

                        rc = ui_mgr_poll_once(mgr, poll_tm);
                        if (rc != 0) {
                                LOGE_T("UI_MGR", "ui_mgr_poll_once failed rc=%d\n", rc);
                                stop_ui_mgr(mgr);
                                mgr->started = 0U;
                                usleep(UI_RUNLOOP_ERR_SLEEP_US);
                        }
                } else {
                        usleep(UI_RUNLOOP_IDLE_SLEEP_US);
                }
        }

        if (mgr->started) {
                stop_ui_mgr(mgr);
                mgr->started = 0U;
        }

        return NULL;
}

int ui_mgr_start_runloop(ui_mgr_t *mgr)
{
        if (!mgr) {
                return -EINVAL;
        }

        if (mgr->runloop_created) {
                return 0;
        }

        mgr->runloop_run = 1U;
        mgr->start_req = 0U;
        mgr->started = 0U;

        pthread_attr_t pth_attr;
        pthread_attr_init(&pth_attr);
        pthread_attr_setscope(&pth_attr, PTHREAD_SCOPE_SYSTEM);
        pthread_attr_setstacksize(&pth_attr, UI_THREAD_STACK_SIZE);
        if (pthread_create(&mgr->runloop_tid, &pth_attr, ui_mgr_runloop_thread_main, mgr) != 0) {
                mgr->runloop_run = 0U;
                return -errno;
        }

        mgr->runloop_created = 1U;
        return 0;
}

int ui_mgr_request_start(ui_mgr_t *mgr)
{
        if (!mgr) {
                return -EINVAL;
        }

        if (!mgr->runloop_created) {
                return -EINVAL;
        }

        mgr->start_req = 1U;
        return 0;
}

int ui_mgr_stop_runloop(ui_mgr_t *mgr)
{
        if (!mgr) {
                return -EINVAL;
        }

        if (!mgr->runloop_created) {
                return 0;
        }

        mgr->runloop_run = 0U;
        pthread_join(mgr->runloop_tid, NULL);
        mgr->runloop_created = 0U;
        return 0;
}