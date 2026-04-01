#ifndef __UI_SNAPSHOT_MGR_H__
#define __UI_SNAPSHOT_MGR_H__

#include <stdint.h>
#include "ui/ui_snapshot_shm.h"

typedef struct ui_snapshot_mgr_s ui_snapshot_mgr_t;

extern ui_snapshot_mgr_t *alloc_ui_snapshot_mgr(void);
extern void destroy_ui_snapshot_mgr(ui_snapshot_mgr_t **mgr);

extern int init_ui_snapshot_mgr(ui_snapshot_mgr_t *mgr, const char *shm_name);
extern void deinit_ui_snapshot_mgr(ui_snapshot_mgr_t *mgr);

extern int publish_ui_snapshot(ui_snapshot_mgr_t *mgr, const ui_snapshot_payload_t *payload, uint32_t *out_seq);

#endif /* __UI_SNAPSHOT_MGR_H__ */