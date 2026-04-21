#ifndef __RA_GIO_GIO_SHM_RA_PRIV_H__
#define __RA_GIO_GIO_SHM_RA_PRIV_H__

#include "ra/gio/gio_shm_ra.h"

struct gio_shm_ra_s {
        gio_shm_ra_cfg_t cfg;
        gio_shm_t shm;
        gio_sem_t sem;
        uint32_t connected;
};

#endif /* __RA_GIO_GIO_SHM_RA_PRIV_H__ */