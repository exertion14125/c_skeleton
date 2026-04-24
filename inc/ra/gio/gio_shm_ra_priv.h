#ifndef __RA_GIO_GIO_SHM_RA_PRIV_H__
#define __RA_GIO_GIO_SHM_RA_PRIV_H__

#include "ra/gio/gio_shm_ra.h"
#include "util/ipc/shm_dbuf.h"

struct gio_shm_ra_s {
        gio_shm_ra_cfg_t cfg;
        void *base_ptr;
        size_t size;
        shm_dbuf_view_t input_view;
        shm_dbuf_view_t output_view;
        gio_shm_ctrl_t ctrl_cache;
        gio_sem_t sem;
        uint32_t connected;
};

#endif /* __RA_GIO_GIO_SHM_RA_PRIV_H__ */