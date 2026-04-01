#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "mgr/ui/ui_mgr_priv.h"
#include "mgr/ui/ui_mgr.h"
#include "util/log/log_util.h"


static void *ui_mgr_runloop_main(void *arg)
{
        ui_mgr_t *mgr = (ui_mgr_t *)arg;
        int rc;

        if (!mgr) {
                return NULL;
        }

        while (mgr->runloop_run) {
                if (mgr->start_req && !mgr->started) {
                        mgr->start_req = 0;

                        rc = start_ui_mgr(mgr);
                        if (rc == 0) {
                                mgr->started = 1;
                                LOGI_T("UI_MGR", "ui_mgr runloop started UI manager.\n");
                        } else {
                                LOGE_T("UI_MGR", "start_ui_mgr failed rc=%d\n", rc);
                                usleep(100 * 1000);
                        }
                }

                if (mgr->started) {
                        rc = ui_mgr_poll_once(mgr, mgr->cfg.poll_timeout_ms > 0
                                                   ? mgr->cfg.poll_timeout_ms
                                                   : 100);
                        if (rc != 0) {
                                LOGE_T("UI_MGR", "ui_mgr_poll_once failed rc=%d\n", rc);
                                stop_ui_mgr(mgr);
                                mgr->started = 0;
                                usleep(100 * 1000);
                        }
                } else {
                        usleep(100 * 1000);
                }
        }

        if (mgr->started) {
                stop_ui_mgr(mgr);
                mgr->started = 0;
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

        mgr->runloop_run = 1;
        mgr->start_req = 0;
        mgr->started = 0;

        if (pthread_create(&mgr->runloop_tid, NULL, ui_mgr_runloop_main, mgr) != 0) {
                mgr->runloop_run = 0;
                return -errno;
        }

        mgr->runloop_created = 1;
        return 0;
}

int ui_mgr_request_start(ui_mgr_t *mgr)
{
        if (!mgr) {
                return -EINVAL;
        }

        mgr->start_req = 1;
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

        mgr->runloop_run = 0;
        pthread_join(mgr->runloop_tid, NULL);
        mgr->runloop_created = 0;
        return 0;
}