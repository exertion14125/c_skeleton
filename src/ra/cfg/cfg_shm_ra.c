#include <stdlib.h>
#include <string.h>

#include "ra/cfg/cfg_shm_ra.h"
#include "ra/gio/gio_shm_owner_ra.h"
#include "util/ipc/shm_dbuf.h"
#include "util/ipc/shm_layout.h"

struct cfg_shm_ra_s {
        cfg_shm_ra_cfg_t cfg;
        void *base_ptr;
        size_t size;
        shm_dbuf_view_t dbuf;
        uint32_t connected;
};

cfg_shm_ra_t *alloc_cfg_shm_ra(void)
{
        return (cfg_shm_ra_t *)calloc(1, sizeof(cfg_shm_ra_t));
}

void destroy_cfg_shm_ra(cfg_shm_ra_t **pra)
{
        cfg_shm_ra_t *ra;

        if (!pra || !*pra) {
                return;
        }

        ra = *pra;
        *pra = NULL;

        deinit_cfg_shm_ra(ra);
        free(ra);
}

int init_cfg_shm_ra(cfg_shm_ra_t *ra, const cfg_shm_ra_cfg_t *cfg)
{
        if (!ra) {
                return -1;
        }

        memset(ra, 0, sizeof(*ra));
        if (cfg) {
                ra->cfg = *cfg;
        }

        if (ra->cfg.shm_name == NULL) {
                ra->cfg.shm_name = "/skeleton_gio_shm";
        }

        return 0;
}

void deinit_cfg_shm_ra(cfg_shm_ra_t *ra)
{
        if (!ra) {
                return;
        }
        cfg_shm_ra_disconnect(ra);
}

int cfg_shm_ra_set_base(cfg_shm_ra_t *ra, void *base_ptr, size_t size)
{
        if (!ra || base_ptr == NULL || size < sizeof(system_shm_t)) {
                return -1;
        }

        ra->base_ptr = base_ptr;
        ra->size = size;
        return 0;
}

int cfg_shm_ra_connect(cfg_shm_ra_t *ra)
{
        system_shm_t *shm;

        if (!ra) {
                return -1;
        }
        if (ra->connected) {
                return 0;
        }

        if (ra->base_ptr == NULL) {
                if (gio_shm_owner_ra_get_shared_base(ra->cfg.shm_name, &ra->base_ptr, &ra->size) != 0) {
                        return -1;
                }
        }
        if (ra->size < sizeof(system_shm_t)) {
                return -1;
        }

        shm = (system_shm_t *)ra->base_ptr;
        if (shm_dbuf_view_bind(&ra->dbuf,
                               &shm->cfg.ctrl,
                               &shm->cfg.slots[0].hdr,
                               shm->cfg.slots[0].payload,
                               &shm->cfg.slots[1].hdr,
                               shm->cfg.slots[1].payload,
                               SHM_CFG_PAYLOAD_MAX) != SHM_OK) {
                return -1;
        }

        ra->connected = 1U;
        return 0;
}

void cfg_shm_ra_disconnect(cfg_shm_ra_t *ra)
{
        if (!ra) {
                return;
        }
        ra->connected = 0U;
}

int cfg_shm_ra_write_effective_cfg(cfg_shm_ra_t *ra, const cfg_export_dto_t *in)
{
        if (!ra || !in || !ra->connected) {
                return -1;
        }

        return (shm_dbuf_publish(&ra->dbuf,
                                 in,
                                 (uint32_t)sizeof(*in),
                                 0U,
                                 0U) == SHM_OK) ? 0 : -1;
}

int cfg_shm_ra_read_effective_cfg(cfg_shm_ra_t *ra, cfg_export_dto_t *out)
{
        uint32_t sz = 0U;

        if (!ra || !out || !ra->connected) {
                return -1;
        }

        if (shm_dbuf_read_snapshot(&ra->dbuf,
                                   out,
                                   (uint32_t)sizeof(*out),
                                   &sz,
                                   NULL) != SHM_OK) {
                return -1;
        }

        return (sz == sizeof(*out)) ? 0 : -1;
}