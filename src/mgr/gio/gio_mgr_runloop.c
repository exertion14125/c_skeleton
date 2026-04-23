#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "mgr/gio/gio_mgr_priv.h"

#define GIO_THREAD_STACK_SIZE         (64 * 1024) //64Kbyte

#define GIO_RUNLOOP_IDLE_SLEEP_US    (100 * 1000)
#define GIO_RUNLOOP_ERR_SLEEP_US     (100 * 1000)

static void *gio_mgr_runloop_thread_main(void *arg)
{
        gio_mgr_t *m = (gio_mgr_t *)arg;
        int rc;

        if (!m) {
                return NULL;
        }

        while (m->runloop_run) {
                if (m->start_req && !m->started) {
                        m->start_req = 0U;
                        rc = start_gio_mgr(m);
                        if (rc == 0) {
                                m->started = 1U;
                        } else {
                                usleep(GIO_RUNLOOP_ERR_SLEEP_US);
                        }
                }

                if (m->started) {
                        rc = gio_mgr_poll_once(m, m->cfg.poll_timeout_ms);
                        if (rc != 0) {
                                stop_gio_mgr(m);
                                m->started = 0U;
                                usleep(GIO_RUNLOOP_ERR_SLEEP_US);
                        }
                } else {
                        usleep(GIO_RUNLOOP_IDLE_SLEEP_US);
                }
        }

        if (m->started) {
                stop_gio_mgr(m);
                m->started = 0U;
        }

        return NULL;
}

int gio_mgr_start_runloop(gio_mgr_t *m)
{
        if (!m) {
                return -EINVAL;
        }

        if (m->runloop_created) {
                return 0;
        }

        m->runloop_run = 1U;
        m->start_req = 0U;
        m->started = 0U;

        pthread_attr_t pth_attr;
        pthread_attr_init(&pth_attr);
        pthread_attr_setscope(&pth_attr, PTHREAD_SCOPE_SYSTEM);
        pthread_attr_setstacksize(&pth_attr, GIO_THREAD_STACK_SIZE);
        if (pthread_create(&m->runloop_tid, &pth_attr, gio_mgr_runloop_thread_main, m) != 0) {
                m->runloop_run = 0U;
                return -errno;
        }

        m->runloop_created = 1U;
        return 0;
}

int gio_mgr_request_start(gio_mgr_t *m)
{
        if (!m) {
                return -EINVAL;
        }

        if (!m->runloop_created) {
                return -EINVAL;
        }

        m->start_req = 1U;
        return 0;
}

int gio_mgr_stop_runloop(gio_mgr_t *m)
{
        if (!m) {
                return -EINVAL;
        }

        if (!m->runloop_created) {
                return 0;
        }

        m->runloop_run = 0U;
        pthread_join(m->runloop_tid, NULL);
        m->runloop_created = 0U;
        return 0;
}