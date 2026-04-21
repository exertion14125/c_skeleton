#include <stdlib.h>
#include <string.h>

#include "ra/gio/gio_shm_ra_priv.h"

static void *gio_shm_ra_get_base_ptr(gio_shm_ra_t *ra)
{
        if (!ra) {
                return NULL;
        }

        /*
         * TODO:
         *   실제 gio_ipc_shm 구현의 mapped pointer 필드명으로 교체 필요.
         *   현재는 임시로 mem 필드를 사용.
         */
        return (void *)ra->shm.mem;
}

gio_shm_ra_t *alloc_gio_shm_ra(void)
{
        gio_shm_ra_t *ra = (gio_shm_ra_t *)calloc(1, sizeof(*ra));
        return ra;
}

void destroy_gio_shm_ra(gio_shm_ra_t **pra)
{
        gio_shm_ra_t *ra;

        if (!pra || !*pra) {
                return;
        }

        ra = *pra;
        *pra = NULL;

        deinit_gio_shm_ra(ra);
        free(ra);
}

int init_gio_shm_ra(gio_shm_ra_t *ra, const gio_shm_ra_cfg_t *cfg)
{
        if (!ra) {
                return -1;
        }

        memset(ra, 0, sizeof(*ra));

        if (cfg) {
                ra->cfg = *cfg;
        }

        if (!ra->cfg.shm_name) {
                ra->cfg.shm_name = "/skeleton_gio_shm";
        }
        if (!ra->cfg.req_sem_name) {
                ra->cfg.req_sem_name = "/skeleton_gio_req_sem";
        }
        if (!ra->cfg.rsp_sem_name) {
                ra->cfg.rsp_sem_name = "/skeleton_gio_rsp_sem";
        }
        if (ra->cfg.max_retry == 0U) {
                ra->cfg.max_retry = 2U;
        }
        if (ra->cfg.timeout_ms == 0U) {
                ra->cfg.timeout_ms = 100U;
        }

        return 0;
}

void deinit_gio_shm_ra(gio_shm_ra_t *ra)
{
        if (!ra) {
                return;
        }

        gio_shm_ra_disconnect(ra);
}

int gio_shm_ra_connect(gio_shm_ra_t *ra)
{
        gio_sem_cfg_t scfg;

        if (!ra) {
                return -1;
        }
        if (ra->connected) {
                return 0;
        }

        memset(&scfg, 0, sizeof(scfg));
        scfg.req_name = ra->cfg.req_sem_name;
        scfg.rsp_name = ra->cfg.rsp_sem_name;
        scfg.req_init = ra->cfg.req_sem_init;
        scfg.rsp_init = ra->cfg.rsp_sem_init;

        if (gio_shm_open(&ra->shm, ra->cfg.shm_name, 0U) != 0) {
                return -1;
        }
        if (gio_shm_map(&ra->shm) != 0) {
                (void)gio_shm_close(&ra->shm);
                return -1;
        }
        if (gio_sem_open(&ra->sem, &scfg) != 0) {
                (void)gio_shm_close(&ra->shm);
                return -1;
        }

        ra->connected = 1U;
        return 0;
}

void gio_shm_ra_disconnect(gio_shm_ra_t *ra)
{
        if (!ra || !ra->connected) {
                return;
        }

        (void)gio_sem_close(&ra->sem);
        (void)gio_shm_close(&ra->shm);
        ra->connected = 0U;
}

int gio_shm_ra_exec(gio_shm_ra_t *ra,
                    const gio_shm_ra_exec_req_t *req,
                    gio_shm_ra_exec_rsp_t *rsp)
{
        uint32_t n;
        int rc = -1;
        gio_ipc_req_t raw_req;
        gio_ipc_rsp_t raw_rsp;

        if (!ra || !req || !rsp) {
                return -1;
        }
        if (!ra->connected) {
                return -1;
        }

        memset(&raw_req, 0, sizeof(raw_req));
        raw_req.req_id = req->req_id;
        raw_req.arg = req->arg;

        memset(rsp, 0, sizeof(*rsp));

        for (n = 0; n <= ra->cfg.max_retry; ++n) {
                if (gio_shm_write_req(&ra->shm, &raw_req) != 0) {
                        rc = -1;
                        continue;
                }
                if (gio_sem_post_req(&ra->sem) != 0) {
                        rc = -1;
                        continue;
                }
                rc = gio_sem_wait_rsp(&ra->sem, ra->cfg.timeout_ms);
                if (rc == 0) {
                        if (gio_shm_read_rsp(&ra->shm, &raw_rsp) == 0) {
                                rsp->rc = raw_rsp.rc;
                                return 0;
                        }
                        rc = -1;
                }
        }

        rsp->rc = rc;
        return -1;
}

int gio_shm_ra_read_all(gio_shm_ra_t *ra, gio_shared_memory_t *out)
{
        void *base;

        if (!ra || !out || !ra->connected) {
                return -1;
        }

        base = gio_shm_ra_get_base_ptr(ra);
        if (!base) {
                return -1;
        }

        memcpy(out, base, sizeof(*out));
        return 0;
}

int gio_shm_ra_write_all(gio_shm_ra_t *ra, const gio_shared_memory_t *in)
{
        void *base;

        if (!ra || !in || !ra->connected) {
                return -1;
        }

        base = gio_shm_ra_get_base_ptr(ra);
        if (!base) {
                return -1;
        }

        memcpy(base, in, sizeof(*in));
        return 0;
}

int gio_shm_ra_read_ctrl(gio_shm_ra_t *ra, gio_shm_ctrl_t *out)
{
        gio_shared_memory_t shm_data;

        if (!out) {
                return -1;
        }
        if (gio_shm_ra_read_all(ra, &shm_data) != 0) {
                return -1;
        }

        *out = shm_data.ctrl;
        return 0;
}

int gio_shm_ra_write_ctrl(gio_shm_ra_t *ra, const gio_shm_ctrl_t *in)
{
        gio_shared_memory_t shm_data;

        if (!in) {
                return -1;
        }
        if (gio_shm_ra_read_all(ra, &shm_data) != 0) {
                return -1;
        }

        shm_data.ctrl = *in;
        return gio_shm_ra_write_all(ra, &shm_data);
}

int gio_shm_ra_read_input(gio_shm_ra_t *ra, gio_input_snapshot_t *out)
{
        gio_shared_memory_t shm_data;

        if (!out) {
                return -1;
        }
        if (gio_shm_ra_read_all(ra, &shm_data) != 0) {
                return -1;
        }

        *out = shm_data.input;
        return 0;
}

int gio_shm_ra_write_output(gio_shm_ra_t *ra, const gio_output_snapshot_t *in)
{
        gio_shared_memory_t shm_data;

        if (!in) {
                return -1;
        }
        if (gio_shm_ra_read_all(ra, &shm_data) != 0) {
                return -1;
        }

        shm_data.output = *in;
        return gio_shm_ra_write_all(ra, &shm_data);
}