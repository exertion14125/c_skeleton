#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "mgr/logic/logic_mgr_priv.h"

#define LOGIC_RUNLOOP_IDLE_SLEEP_US    (100 * 1000)
#define LOGIC_RUNLOOP_ERR_SLEEP_US     (100 * 1000)

static void *logic_mgr_runloop_thread_main(void *arg)
{
        logic_mgr_t *m = (logic_mgr_t *)arg;
        int rc;

        if (!m) {
                return NULL;
        }

        while (m->runloop_run) {
                if (m->start_req && !m->started) {
                        m->start_req = 0U;
                        rc = start_logic_mgr(m);
                        if (rc == 0) {
                                m->started = 1U;
                        } else {
                                usleep(LOGIC_RUNLOOP_ERR_SLEEP_US);
                        }
                }

                if (m->started) {
                        rc = logic_mgr_poll_once(m, m->cfg.poll_timeout_ms);
                        if (rc != 0) {
                                stop_logic_mgr(m);
                                m->started = 0U;
                                usleep(LOGIC_RUNLOOP_ERR_SLEEP_US);
                        }
                } else {
                        usleep(LOGIC_RUNLOOP_IDLE_SLEEP_US);
                }
        }

        if (m->started) {
                stop_logic_mgr(m);
                m->started = 0U;
        }

        return NULL;
}

int logic_mgr_start_runloop(logic_mgr_t *m)
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

        if (pthread_create(&m->runloop_tid, NULL, logic_mgr_runloop_thread_main, m) != 0) {
                m->runloop_run = 0U;
                return -errno;
        }

        m->runloop_created = 1U;
        return 0;
}

int logic_mgr_request_start(logic_mgr_t *m)
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

int logic_mgr_stop_runloop(logic_mgr_t *m)
{
        if (!m) {
                return -EINVAL;
        }

        if (!m->runloop_created) {
                return 0;
        }

        m->runloop_run = 0U;

        (void)pthread_join(m->runloop_tid, NULL);

        m->runloop_created = 0U;
        m->start_req = 0U;
        m->started = 0U;

        return 0;
}