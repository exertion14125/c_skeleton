#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "ra/ui/ra_ui_shm.h"

/// @brief RA UI SHM context structure.
struct ra_ui_shm_s {
        int fd;         ///< SHM file descriptor.
        int is_writer;  ///< 1 if this is a writer (creator), 0 if reader.
        size_t size;    ///< Size of the SHM region.
        char name[64];  ///< Name of the SHM object.
        ui_snapshot_shm_layout_t *ptr; ///< Pointer to the mapped SHM region (ui_snapshot_shm_layout_t).
};

/// @brief Allocate RA UI SHM context.
/// @return Pointer to the allocated RA UI SHM context, or NULL on failure.
ra_ui_shm_t *ra_ui_shm_alloc(void)
{
        return (ra_ui_shm_t *)calloc(1, sizeof(ra_ui_shm_t));
}

/// @brief Destroy RA UI SHM context.
/// @param pshm Pointer to RA UI SHM context pointer.
void ra_ui_shm_destroy(ra_ui_shm_t **pshm)
{
        if (!pshm || !*pshm) {
                return;
        }
        ra_ui_shm_deinit(*pshm);
        free(*pshm);
        *pshm = NULL;
}

/// @brief Common function to initialize RA UI SHM for both writer and reader.
/// @param shm RA UI SHM context.
/// @param name Name of the SHM object.
/// @param size Size of the SHM region (must be >= sizeof(ui_snapshot_shm_layout_t)).
/// @param oflag Flags for shm_open (e.g., O_CREAT | O_RDWR for writer, O_RDWR for reader).
/// @param is_writer 1 if this is a writer (creator), 0 if reader.
/// @return 0 on success, negative errno on failure.
static int ra_ui_shm_map_common(ra_ui_shm_t *shm, const char *name, size_t size, int oflag, int is_writer)
{
        if (!shm || !name || size < sizeof(ui_snapshot_shm_layout_t)) {
                return -EINVAL;
        }

        memset(shm, 0, sizeof(*shm));
        shm->fd = -1;
        shm->size = size;
        shm->is_writer = is_writer;
        strncpy(shm->name, name, sizeof(shm->name) - 1);

        shm->fd = shm_open(name, oflag, 0660);
        if (shm->fd < 0) {
                return -errno;
        }

        if (is_writer) {
                if (ftruncate(shm->fd, (off_t)size) != 0) {
                        int e = -errno;
                        close(shm->fd);
                        shm->fd = -1;
                        return e;
                }
        }

        shm->ptr = (ui_snapshot_shm_layout_t *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
        if (shm->ptr == MAP_FAILED) {
                int e = -errno;
                close(shm->fd);
                shm->fd = -1;
                shm->ptr = NULL;
                return e;
        }

        if (is_writer) {
                memset(shm->ptr, 0, sizeof(ui_snapshot_shm_layout_t));
                shm->ptr->hdr.magic = UI_SNAPSHOT_MAGIC;
                shm->ptr->hdr.version = UI_SNAPSHOT_VERSION;
                shm->ptr->hdr.hdr_size = sizeof(ui_snapshot_hdr_t);
                shm->ptr->hdr.total_size = sizeof(ui_snapshot_shm_layout_t);
                shm->ptr->hdr.active_idx = 0;
                shm->ptr->hdr.seq = 0;
        }

        return 0;
}

/// @brief Initialize RA UI SHM for writer (creator).
/// @param shm RA UI SHM context.
/// @param name Name of the SHM object.
/// @param size Size of the SHM region (must be >= sizeof(ui_snapshot_shm_layout_t)).
/// @return 0 on success, negative errno on failure.
int ra_ui_shm_init_writer(ra_ui_shm_t *shm, const char *name, size_t size)
{
        return ra_ui_shm_map_common(shm, name, size, O_CREAT | O_RDWR, 1);
}

/// @brief Initialize RA UI SHM for reader.
/// @param shm RA UI SHM context.
/// @param name Name of the SHM object.
/// @param size Size of the SHM region (must be >= sizeof(ui_snapshot_shm_layout_t)).
/// @return 0 on success, negative errno on failure.
int ra_ui_shm_init_reader(ra_ui_shm_t *shm, const char *name, size_t size)
{
        return ra_ui_shm_map_common(shm, name, size, O_RDWR, 0);
}

/// @brief Deinitialize RA UI SHM (unmap and close).
/// @param shm RA UI SHM context.
void ra_ui_shm_deinit(ra_ui_shm_t *shm)
{
        if (!shm) {
                return;
        }

        if (shm->ptr) {
                munmap((void *)shm->ptr, shm->size);
                shm->ptr = NULL;
        }

        if (shm->fd >= 0) {
                close(shm->fd);
                shm->fd = -1;
        }

        memset(shm, 0, sizeof(*shm));
        shm->fd = -1;
}

/// @brief Commit a UI snapshot to SHM (writer only).
/// @param shm RA UI SHM context (must be initialized as writer).
/// @param payload Pointer to the UI snapshot payload to commit.
/// @return 0 on success, negative errno on failure.
int ra_ui_shm_commit_snapshot(ra_ui_shm_t *shm, const ui_snapshot_payload_t *payload)
{
        ui_snapshot_shm_layout_t *p;
        uint32_t next_idx;
        uint32_t next_seq;

        if (!shm || !shm->ptr || !payload || !shm->is_writer) {
                return -EINVAL;
        }

        p = shm->ptr;
        next_idx = (p->hdr.active_idx == 0u) ? 1u : 0u;

        memcpy(&p->buf[next_idx], payload, sizeof(*payload));

        next_seq = p->hdr.seq + 1u;
        p->hdr.active_idx = next_idx;
        p->hdr.seq = next_seq;

        return 0;
}

/// @brief Read a UI snapshot from SHM (reader only).
/// @param shm RA UI SHM context (must be initialized as reader).
/// @param out_payload Pointer to the buffer to store the UI snapshot payload.
/// @param out_seq Pointer to the variable to store the sequence number (optional).
/// @return 0 on success, negative errno on failure.
int ra_ui_shm_read_snapshot(ra_ui_shm_t *shm, ui_snapshot_payload_t *out_payload, uint32_t *out_seq)
{
        ui_snapshot_shm_layout_t *p;
        uint32_t seq1, seq2, idx;

        if (!shm || !shm->ptr || !out_payload) {
                return -EINVAL;
        }

        p = shm->ptr;

        do {
                seq1 = p->hdr.seq;
                idx = p->hdr.active_idx;
                memcpy(out_payload, &p->buf[idx], sizeof(*out_payload));
                seq2 = p->hdr.seq;
        } while (seq1 != seq2);

        if (out_seq) {
                *out_seq = seq2;
        }

        return 0;
}

/// @brief Get the current sequence number of the UI snapshot in SHM (reader only).
/// @param shm RA UI SHM context (must be initialized as reader).
/// @param out_seq Pointer to the variable to store the sequence number.
/// @return 0 on success, negative errno on failure.
int ra_ui_shm_get_seq(ra_ui_shm_t *shm, uint32_t *out_seq)
{
        if (!shm || !shm->ptr || !out_seq) {
                return -EINVAL;
        }

        *out_seq = shm->ptr->hdr.seq;
        return 0;
}