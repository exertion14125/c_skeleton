#ifndef __RA_UI_SHM_H__
#define __RA_UI_SHM_H__

#include <stddef.h>
#include <stdint.h>
#include "ui/ui_snapshot_shm.h"

typedef struct ra_ui_shm_s ra_ui_shm_t;

extern ra_ui_shm_t *ra_ui_shm_alloc(void);
extern void ra_ui_shm_destroy(ra_ui_shm_t **shm);

extern int ra_ui_shm_init_writer(ra_ui_shm_t *shm, const char *name, size_t size);
extern int ra_ui_shm_init_reader(ra_ui_shm_t *shm, const char *name, size_t size);
extern void ra_ui_shm_deinit(ra_ui_shm_t *shm);

extern int ra_ui_shm_commit_snapshot(ra_ui_shm_t *shm, const ui_snapshot_payload_t *payload);
extern int ra_ui_shm_read_snapshot(ra_ui_shm_t *shm, ui_snapshot_payload_t *out_payload, uint32_t *out_seq);
extern int ra_ui_shm_get_seq(ra_ui_shm_t *shm, uint32_t *out_seq);
#endif /* __RA_UI_SHM_H__ */