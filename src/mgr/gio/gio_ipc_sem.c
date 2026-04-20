#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include "mgr/gio/gio_ipc_sem.h"

int gio_sem_open(gio_sem_t *s, const gio_sem_cfg_t *cfg)
{
        unsigned int req_init;
        unsigned int rsp_init;

        if (!s || !cfg || !cfg->req_name || !cfg->rsp_name) {
                return -EINVAL;
        }

        memset(s, 0, sizeof(*s));
        req_init = cfg->req_init;
        rsp_init = cfg->rsp_init;

        s->req = sem_open(cfg->req_name, O_CREAT, 0660, req_init);
        if (s->req == SEM_FAILED) {
                s->req = NULL;
                return -errno;
        }

        s->rsp = sem_open(cfg->rsp_name, O_CREAT, 0660, rsp_init);
        if (s->rsp == SEM_FAILED) {
                int err = errno;
                sem_close(s->req);
                s->req = NULL;
                s->rsp = NULL;
                return -err;
        }

        strncpy(s->req_name, cfg->req_name, sizeof(s->req_name) - 1U);
        strncpy(s->rsp_name, cfg->rsp_name, sizeof(s->rsp_name) - 1U);
        s->req_name[sizeof(s->req_name) - 1U] = '\0';
        s->rsp_name[sizeof(s->rsp_name) - 1U] = '\0';
        return 0;
}

int gio_sem_post_req(gio_sem_t *s)
{
        if (!s || !s->req) {
                return -EINVAL;
        }

        if (sem_post(s->req) != 0) {
                return -errno;
        }

        return 0;
}

int gio_sem_wait_rsp(gio_sem_t *s, uint32_t timeout_ms)
{
        struct timespec ts;

        if (!s || !s->rsp) {
                return -EINVAL;
        }

        if (timeout_ms == 0U) {
                if (sem_wait(s->rsp) != 0) {
                        return -errno;
                }
                return 0;
        }

        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += (time_t)(timeout_ms / 1000U);
        ts.tv_nsec += (long)((timeout_ms % 1000U) * 1000000UL);
        if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000L;
        }

        if (sem_timedwait(s->rsp, &ts) != 0) {
                return -errno;
        }

        return 0;
}

int gio_sem_close(gio_sem_t *s)
{
        if (!s) {
                return -EINVAL;
        }

        if (s->req) {
                sem_close(s->req);
                s->req = NULL;
        }
        if (s->rsp) {
                sem_close(s->rsp);
                s->rsp = NULL;
        }

        return 0;
}