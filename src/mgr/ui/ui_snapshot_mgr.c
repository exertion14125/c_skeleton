#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mgr/ui/ui_snapshot_mgr.h"
#include "ra/ui/ra_ui_shm.h"

/// @brief UI snapshot manager context structure.
struct ui_snapshot_mgr_s {
        ra_ui_shm_t *shm;
};

/// @brief Allocate UI snapshot manager.
/// @param mgr Pointer to UI snapshot manager pointer.
/// @return Allocated UI snapshot manager, or NULL on failure.
ui_snapshot_mgr_t *alloc_ui_snapshot_mgr(void)
{
        return (ui_snapshot_mgr_t *)calloc(1, sizeof(ui_snapshot_mgr_t));
}

/// @brief Destroy UI snapshot manager.
/// @param pmgr Pointer to UI snapshot manager pointer.
void destroy_ui_snapshot_mgr(ui_snapshot_mgr_t **pmgr)
{
        if (!pmgr || !*pmgr) {
                return;
        }
        deinit_ui_snapshot_mgr(*pmgr);
        free(*pmgr);
        *pmgr = NULL;
}

/// @brief Initialize UI snapshot manager.
/// @param mgr UI snapshot manager.
/// @param shm_name Name of the SHM object for UI snapshots.
/// @return 0 on success, negative errno on failure.
int init_ui_snapshot_mgr(ui_snapshot_mgr_t *mgr, const char *shm_name)
{
        if (!mgr || !shm_name) {
                return -EINVAL;
        }

        memset(mgr, 0, sizeof(*mgr));

        mgr->shm = ra_ui_shm_alloc();
        if (!mgr->shm) {
                return -ENOMEM;
        }

        return ra_ui_shm_init_writer(mgr->shm, shm_name, sizeof(ui_snapshot_shm_layout_t));
}

/// @brief Deinitialize UI snapshot manager.
/// @param mgr UI snapshot manager.
void deinit_ui_snapshot_mgr(ui_snapshot_mgr_t *mgr)
{
        if (!mgr) {
                return;
        }

        if (mgr->shm) {
                ra_ui_shm_destroy(&mgr->shm);
        }
}

/// @brief Publish a UI snapshot to SHM.
/// @param mgr UI snapshot manager.
/// @param payload Pointer to the UI snapshot payload to publish.
/// @param out_seq Pointer to the variable to store the sequence number of the published snapshot (optional).
/// @return 0 on success, negative errno on failure.
int publish_ui_snapshot(ui_snapshot_mgr_t *mgr, const ui_snapshot_payload_t *payload, uint32_t *out_seq)
{
        int rc;

        if (!mgr || !mgr->shm || !payload) {
                return -EINVAL;
        }

        rc = ra_ui_shm_commit_snapshot(mgr->shm, payload);
        if (rc != 0) {
                return rc;
        }

        if (out_seq) {
                rc = ra_ui_shm_get_seq(mgr->shm, out_seq);
                if (rc != 0) {
                        return rc;
                }
        }

        return 0;
}