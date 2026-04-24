#include "util/ipc/shm_dbuf.h"

#include <string.h>

#define SHM_DBUF_SLOT_COUNT 2u
#define SHM_DBUF_MAX_READ_RETRY 3u

static uint32_t shm_dbuf_normalize_idx(uint32_t idx)
{
        return idx & 1u;
}

int shm_dbuf_view_bind(
        shm_dbuf_view_t *view,
        shm_dbuf_ctrl_t *ctrl,
        shm_slot_hdr_t *slot0_hdr,
        uint8_t *slot0_payload,
        shm_slot_hdr_t *slot1_hdr,
        uint8_t *slot1_payload,
        uint32_t payload_capacity)
{
        if (view == NULL || ctrl == NULL || slot0_hdr == NULL || slot1_hdr == NULL ||
                slot0_payload == NULL || slot1_payload == NULL || payload_capacity == 0u) {
                return SHM_EINVAL;
        }

        view->ctrl = ctrl;
        view->slot_hdr[0] = slot0_hdr;
        view->slot_hdr[1] = slot1_hdr;
        view->slot_payload[0] = slot0_payload;
        view->slot_payload[1] = slot1_payload;
        view->payload_capacity = payload_capacity;

        return SHM_OK;
}

int shm_dbuf_ctrl_reset(shm_dbuf_ctrl_t *ctrl, uint32_t payload_size)
{
        if (ctrl == NULL || payload_size == 0u) {
                return SHM_EINVAL;
        }

        ctrl->active_idx = 0u;
        ctrl->publish_seq = 0u;
        ctrl->payload_size = payload_size;
        ctrl->flags = 0u;

        shm_memory_barrier_full();
        return SHM_OK;
}

int shm_dbuf_publish(
        shm_dbuf_view_t *view,
        const void *payload,
        uint32_t payload_size,
        uint32_t slot_flags,
        uint32_t ctrl_flags)
{
        uint32_t active_idx;
        uint32_t inactive_idx;
        uint32_t next_seq;
        shm_slot_hdr_t *hdr;

        if (view == NULL || view->ctrl == NULL || payload == NULL) {
                return SHM_EINVAL;
        }
        if (payload_size > view->payload_capacity) {
                return SHM_EBOUNDS;
        }

        active_idx = shm_dbuf_normalize_idx(view->ctrl->active_idx);
        inactive_idx = (active_idx + 1u) % SHM_DBUF_SLOT_COUNT;

        memcpy(view->slot_payload[inactive_idx], payload, payload_size);

        hdr = view->slot_hdr[inactive_idx];
        next_seq = view->ctrl->publish_seq + 1u;
        hdr->size = payload_size;
        hdr->flags = slot_flags;
        hdr->reserved = 0u;
        hdr->seq = next_seq;

        /* Ensure slot payload/header are visible before active index flip. */
        shm_memory_barrier_release();

        view->ctrl->flags = ctrl_flags;
        view->ctrl->payload_size = payload_size;
        view->ctrl->publish_seq = next_seq;
        view->ctrl->active_idx = inactive_idx;

        return SHM_OK;
}

int shm_dbuf_read_snapshot(
        const shm_dbuf_view_t *view,
        void *out_snapshot,
        uint32_t out_capacity,
        uint32_t *out_size,
        uint32_t *out_seq)
{
        uint32_t retry;

        if (view == NULL || view->ctrl == NULL || out_snapshot == NULL || out_size == NULL) {
                return SHM_EINVAL;
        }

        for (retry = 0u; retry < SHM_DBUF_MAX_READ_RETRY; ++retry) {
                uint32_t active_idx_before;
                uint32_t active_idx_after;
                const shm_slot_hdr_t *hdr;
                uint32_t payload_size;
                uint32_t seq_before;

                active_idx_before = shm_dbuf_normalize_idx(view->ctrl->active_idx);
                shm_memory_barrier_acquire();

                hdr = view->slot_hdr[active_idx_before];
                payload_size = hdr->size;
                seq_before = hdr->seq;

                if (payload_size > view->payload_capacity || payload_size > out_capacity) {
                        return SHM_EBOUNDS;
                }

                memcpy(out_snapshot, view->slot_payload[active_idx_before], payload_size);

                shm_memory_barrier_acquire();
                active_idx_after = shm_dbuf_normalize_idx(view->ctrl->active_idx);

                if (active_idx_before == active_idx_after && seq_before == view->slot_hdr[active_idx_after]->seq) {
                        *out_size = payload_size;
                        if (out_seq != NULL) {
                                *out_seq = seq_before;
                        }
                        return SHM_OK;
                }
        }

        return SHM_ESTALE;
}
