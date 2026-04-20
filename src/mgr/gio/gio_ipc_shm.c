#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "mgr/gio/gio_ipc_shm.h"

typedef struct gio_ipc_block_s {
        gio_ipc_req_t req;
        gio_ipc_rsp_t rsp;
} gio_ipc_block_t;

int gio_shm_open(gio_shm_t *s, const char *name, size_t size)
{
        if (!s || !name || name[0] == '\0') {
                return -EINVAL;
        }

        memset(s, 0, sizeof(*s));
        s->fd = shm_open(name, O_CREAT | O_RDWR, 0660);
        if (s->fd < 0) {
                return -errno;
        }

        if (size < sizeof(gio_ipc_block_t)) {
                size = sizeof(gio_ipc_block_t);
        }

        if (ftruncate(s->fd, (off_t)size) != 0) {
                int err = errno;
                close(s->fd);
                s->fd = -1;
                return -err;
        }

        s->size = size;
        strncpy(s->name, name, sizeof(s->name) - 1U);
        s->name[sizeof(s->name) - 1U] = '\0';
        return 0;
}

int gio_shm_map(gio_shm_t *s)
{
        if (!s || s->fd < 0 || s->size == 0U) {
                return -EINVAL;
        }

        s->mem = mmap(NULL, s->size, PROT_READ | PROT_WRITE, MAP_SHARED, s->fd, 0);
        if (s->mem == MAP_FAILED) {
                s->mem = NULL;
                return -errno;
        }

        return 0;
}

int gio_shm_write_req(gio_shm_t *s, const gio_ipc_req_t *req)
{
        gio_ipc_block_t *blk;
        if (!s || !s->mem || !req) {
                return -EINVAL;
        }

        blk = (gio_ipc_block_t *)s->mem;
        blk->req = *req;
        return 0;
}

int gio_shm_read_rsp(gio_shm_t *s, gio_ipc_rsp_t *rsp)
{
        gio_ipc_block_t *blk;
        if (!s || !s->mem || !rsp) {
                return -EINVAL;
        }

        blk = (gio_ipc_block_t *)s->mem;
        *rsp = blk->rsp;
        return 0;
}

int gio_shm_unmap(gio_shm_t *s)
{
        if (!s) {
                return -EINVAL;
        }
        if (s->mem) {
                if (munmap(s->mem, s->size) != 0) {
                        return -errno;
                }
                s->mem = NULL;
        }
        return 0;
}

int gio_shm_close(gio_shm_t *s)
{
        if (!s) {
                return -EINVAL;
        }

        (void)gio_shm_unmap(s);
        if (s->fd >= 0) {
                (void)close(s->fd);
                s->fd = -1;
        }

        return 0;
}