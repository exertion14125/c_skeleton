#include <stdlib.h>
#include <string.h>

#include "ra/gio/gio_shm_owner_ra.h"
#include "ra/gio/gio_shm_ra_priv.h"
#include "util/ipc/shm_dbuf.h"
#include "util/ipc/system_shm_layout.h"

/// @brief Retrieves the base address and size of the shared memory segment for the GIO shared memory owner RA.
/// This function is used by other components to access the shared memory segment managed by the GIO shared memory owner RA. 
/// It checks the registry for a valid entry matching the requested shared memory name and returns the corresponding base address and size if found.
/// @param ra Pointer to the GIO shared memory owner RA instance.
/// @return Pointer to the base address of the shared memory segment, or NULL if not available.
static void *gio_shm_ra_get_base_ptr(gio_shm_ra_t *ra)
{
        if (!ra) {
                return NULL;
        }

        return ra->base_ptr;
}

/// @brief Allocates memory for a GIO shared memory RA instance.
/// @return Pointer to the allocated GIO shared memory RA instance, or NULL if allocation fails.
gio_shm_ra_t *alloc_gio_shm_ra(void)
{
        gio_shm_ra_t *ra = (gio_shm_ra_t *)calloc(1, sizeof(*ra));
        return ra;
}

/// @brief Destroys a GIO shared memory RA instance, freeing its resources.
/// @param pra Pointer to the pointer of the GIO shared memory RA instance to be destroyed. After this function, the pointer will be set to NULL.
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

/// @brief Initializes a GIO shared memory RA instance with the provided configuration.
/// @param ra Pointer to the GIO shared memory RA instance to be initialized.
/// @param cfg Pointer to the configuration for the GIO shared memory RA instance.
/// @return 0 on success, or a negative error code on failure.
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

/// @brief Deinitializes a GIO shared memory RA instance, releasing any resources it holds.
/// @param ra Pointer to the GIO shared memory RA instance to be deinitialized.
/// @param base_ptr Pointer to the base address of the shared memory segment to be set for the GIO shared memory RA instance.
/// @param size Size of the shared memory segment to be set for the GIO shared memory RA instance.
/// @return 0 on success, or a negative error code on failure.
int gio_shm_ra_set_base(gio_shm_ra_t *ra, void *base_ptr, size_t size)
{
        if (!ra || base_ptr == NULL || size == 0U) {
                return -1;
        }

        ra->base_ptr = base_ptr;
        ra->size = size;
        return 0;
}

/// @brief Deinitializes a GIO shared memory RA instance, releasing any resources it holds and disconnecting from the shared memory segment if connected.
/// @param ra Pointer to the GIO shared memory RA instance to be deinitialized.
void deinit_gio_shm_ra(gio_shm_ra_t *ra)
{
        if (!ra) {
                return;
        }

        gio_shm_ra_disconnect(ra);
}

/// @brief Connects a GIO shared memory RA instance to the shared memory segment and semaphores specified in its configuration.
/// @param ra Pointer to the GIO shared memory RA instance to be connected.
/// @return 0 on success, or a negative error code on failure.
int gio_shm_ra_connect(gio_shm_ra_t *ra)
{
        gio_sem_cfg_t scfg;
        system_shm_t *shm;

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

        if (ra->base_ptr == NULL) {
                if (gio_shm_owner_ra_get_shared_base(ra->cfg.shm_name, &ra->base_ptr, &ra->size) != 0) {
                        return -1;
                }
        }
        if (ra->size < sizeof(system_shm_t)) {
                return -1;
        }

        shm = (system_shm_t *)ra->base_ptr;
        if (shm_dbuf_view_bind(&ra->input_view,
                               &shm->gio_in.ctrl,
                               &shm->gio_in.slots[0].hdr,
                               shm->gio_in.slots[0].payload,
                               &shm->gio_in.slots[1].hdr,
                               shm->gio_in.slots[1].payload,
                               SHM_GIO_IN_PAYLOAD_MAX) != SHM_OK) {
                return -1;
        }
        if (shm_dbuf_view_bind(&ra->output_view,
                               &shm->gio_out.ctrl,
                               &shm->gio_out.slots[0].hdr,
                               shm->gio_out.slots[0].payload,
                               &shm->gio_out.slots[1].hdr,
                               shm->gio_out.slots[1].payload,
                               SHM_GIO_OUT_PAYLOAD_MAX) != SHM_OK) {
                return -1;
        }

        if (gio_sem_open(&ra->sem, &scfg) != 0) {
                return -1;
        }

        ra->connected = 1U;
        return 0;
}

/// @brief Disconnects a GIO shared memory RA instance from the shared memory segment and semaphores, releasing any resources it holds.
/// @param ra Pointer to the GIO shared memory RA instance to be disconnected.
void gio_shm_ra_disconnect(gio_shm_ra_t *ra)
{
        if (!ra || !ra->connected) {
                return;
        }

        (void)gio_sem_close(&ra->sem);
        ra->connected = 0U;
}

/// @brief Executes a request using the GIO shared memory RA, sending the request through shared memory and semaphores, and waiting for the response.
/// @param ra Pointer to the GIO shared memory RA instance to be used for executing the request.
/// @param req Pointer to the request structure containing the request ID and argument to be sent.
/// @param rsp Pointer to the response structure where the return code from the response will be stored.
/// @return 0 on success, or a negative error code on failure. The response structure will contain the return code from the response if the operation is successful.
int gio_shm_ra_exec(gio_shm_ra_t *ra, const gio_shm_ra_exec_req_t *req,
                    gio_shm_ra_exec_rsp_t *rsp)
{
        uint32_t n;
        int rc = -1;
        gio_shm_t shm_view;
        gio_ipc_req_t raw_req;
        gio_ipc_rsp_t raw_rsp;
        uint8_t *ipc_base;

        if (!ra || !req || !rsp) {
                return -1;
        }
        if (!ra->connected) {
                return -1;
        }
        if (!ra->base_ptr || ra->size == 0U) {
            return -1;
        }

        memset(&raw_req, 0, sizeof(raw_req));
        raw_req.req_id = req->req_id;
        raw_req.arg = req->arg;

        memset(rsp, 0, sizeof(*rsp));

        memset(&shm_view, 0, sizeof(shm_view));
        ipc_base = (uint8_t *)ra->base_ptr + sizeof(system_shm_t);
        shm_view.mem = ipc_base;
        shm_view.size = ra->size;

        for (n = 0; n <= ra->cfg.max_retry; ++n) {
                if (gio_shm_write_req(&shm_view, &raw_req) != 0) {
                        rc = -1;
                        continue;
                }
                if (gio_sem_post_req(&ra->sem) != 0) {
                        rc = -1;
                        continue;
                }
                rc = gio_sem_wait_rsp(&ra->sem, ra->cfg.timeout_ms);
                if (rc == 0) {
                        if (gio_shm_read_rsp(&shm_view, &raw_rsp) == 0) {
                                rsp->rc = raw_rsp.rc;
                                return 0;
                        }
                        rc = -1;
                }
        }

        rsp->rc = rc;
        return -1;
}

/// @brief Reads the entire shared memory content for the GIO shared memory RA, including control data, input snapshot, and output snapshot, and stores it in the provided output structure.
/// @param ra Pointer to the GIO shared memory RA instance from which to read the shared memory content.
/// @param out Pointer to the structure where the read shared memory content will be stored. This structure will be populated with the control data, input snapshot, and output snapshot read from the shared memory.
/// @return 0 on success, or a negative error code on failure. The output structure will contain the read shared memory content if the operation is successful.
int gio_shm_ra_read_all(gio_shm_ra_t *ra, gio_shared_memory_t *out)
{
        uint32_t sz = 0U;

        if (!ra || !out || !ra->connected) {
                return -1;
        }

        memset(out, 0, sizeof(*out));
        out->ctrl = ra->ctrl_cache;

        if (shm_dbuf_read_snapshot(&ra->input_view,
                                   &out->input,
                                   (uint32_t)sizeof(out->input),
                                   &sz,
                                   NULL) != SHM_OK) {
                return -1;
        }
        if (sz != sizeof(out->input)) {
                return -1;
        }

        if (shm_dbuf_read_snapshot(&ra->output_view,
                                   &out->output,
                                   (uint32_t)sizeof(out->output),
                                   &sz,
                                   NULL) != SHM_OK) {
                return -1;
        }
        if (sz != sizeof(out->output)) {
                return -1;
        }

        return 0;
}

/// @brief Writes the entire shared memory content for the GIO shared memory RA, including control data, input snapshot, and output snapshot, from the provided input structure.
/// @param ra Pointer to the GIO shared memory RA instance to which to write the shared memory content.
/// @param in Pointer to the structure containing the shared memory content to be written. This structure
///        should be populated with the control data, input snapshot, and output snapshot that will be written to the shared memory.
/// @return 0 on success, or a negative error code on failure. The shared memory will be updated with the content from the input structure if the operation is successful.
int gio_shm_ra_write_all(gio_shm_ra_t *ra, const gio_shared_memory_t *in)
{
        if (!ra || !in || !ra->connected) {
                return -1;
        }

        ra->ctrl_cache = in->ctrl;
        if (gio_shm_ra_write_output_snapshot(ra, &in->output) != 0) {
                return -1;
        }

        return 0;
}

/// @brief Reads the control data from the shared memory for the GIO shared memory RA and stores it in the provided output structure.
/// @param ra Pointer to the GIO shared memory RA instance from which to read the control data.
/// @param out Pointer to the structure where the read control data will be stored. This structure will be populated with the control data read from the shared memory.
/// @return 0 on success, or a negative error code on failure. The output structure will contain the read control data if the operation is successful.
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

/// @brief Writes the control data to the shared memory for the GIO shared memory RA from the provided input structure.
/// @param ra Pointer to the GIO shared memory RA instance to which to write the control data.
/// @param in Pointer to the structure containing the control data to be written. This structure should be populated with the control data that will be written to the shared memory.
/// @return 0 on success, or a negative error code on failure. The shared memory will be updated with the control data from the input structure if the operation is successful.
int gio_shm_ra_write_ctrl(gio_shm_ra_t *ra, const gio_shm_ctrl_t *in)
{
        if (!in) {
                return -1;
        }

        ra->ctrl_cache = *in;
        return 0;
}

/// @brief Reads the input snapshot from the shared memory for the GIO shared memory RA and stores it in the provided output structure.
/// @param ra Pointer to the GIO shared memory RA instance from which to read the input snapshot.
/// @param out Pointer to the structure where the read input snapshot will be stored. This structure will be populated with the input snapshot read from the shared memory.
/// @return 0 on success, or a negative error code on failure. The output structure will contain the read input snapshot if the operation is successful.
int gio_shm_ra_read_input(gio_shm_ra_t *ra, gio_input_snapshot_t *out)
{
        return gio_shm_ra_read_input_snapshot(ra, out, NULL);
}

/// @brief Writes the output snapshot to the shared memory for the GIO shared memory RA from the provided input structure.
/// @param ra Pointer to the GIO shared memory RA instance to which to write the output snapshot.
/// @param in Pointer to the structure containing the output snapshot to be written. This structure should be populated with the output snapshot that will be written to the shared memory.
/// @return 0 on success, or a negative error code on failure. The shared memory will be updated with the output snapshot from the input structure if the operation is successful.
int gio_shm_ra_write_output(gio_shm_ra_t *ra, const gio_output_snapshot_t *in)
{
        return gio_shm_ra_write_output_snapshot(ra, in);
}

/// @brief Reads the input snapshot from the shared memory for the GIO shared memory RA and stores it in the provided output structure, along with the sequence number of the snapshot if requested.
/// @param ra Pointer to the GIO shared memory RA instance from which to read the input snapshot.
/// @param out Pointer to the structure where the read input snapshot will be stored. This structure will be populated with the input snapshot read from the shared memory.
/// @param out_seq Optional pointer to a variable where the sequence number of the read snapshot will be stored. If this parameter is not NULL, it will be set to the sequence number of the snapshot that was read from the shared memory.
/// @return 0 on success, or a negative error code on failure. The output structure will contain the read input snapshot if the operation is successful, and the out_seq variable will contain the sequence number of the snapshot if out_seq is not NULL.
int gio_shm_ra_read_input_snapshot(gio_shm_ra_t *ra, gio_input_snapshot_t *out, uint32_t *out_seq)
{
        uint32_t sz = 0U;
        uint32_t seq = 0U;

        if (!ra || !out || !ra->connected) {
                return -1;
        }

        if (shm_dbuf_read_snapshot(&ra->input_view,
                                   out,
                                   (uint32_t)sizeof(*out),
                                   &sz,
                                   &seq) != SHM_OK) {
                return -1;
        }
        if (sz != sizeof(*out)) {
                return -1;
        }
        if (out_seq) {
                *out_seq = seq;
        }
        return 0;
}

/// @brief Writes the output snapshot to the shared memory for the GIO shared memory RA from the provided input structure.
/// @param ra Pointer to the GIO shared memory RA instance to which to write the output snapshot.
/// @param in Pointer to the structure containing the output snapshot to be written. This structure should be populated with the output snapshot that will be written to the shared memory.
/// @return 0 on success, or a negative error code on failure. The shared memory will be updated with the output snapshot from the input structure if the operation is successful.
int gio_shm_ra_write_output_snapshot(gio_shm_ra_t *ra, const gio_output_snapshot_t *in)
{
        if (!ra || !in || !ra->connected) {
                return -1;
        }

        return (shm_dbuf_publish(&ra->output_view,
                                 in,
                                 (uint32_t)sizeof(*in),
                                 0U,
                                 0U) == SHM_OK) ? 0 : -1;
}