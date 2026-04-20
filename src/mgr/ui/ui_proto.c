#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "mgr/ui/ui_proto.h"

/// @brief Move the remaining data in the RX buffer to the left after consuming a frame.
/// @param rb Pointer to the UI RX buffer.
/// @param nbytes Number of bytes to move left (consumed frame length).
/// @return 0 on success, negative errno on failure.
static int ui_proto_memmove_left(ui_rx_buf_t *rb, size_t nbytes)
{
        if (!rb) {
                return -EINVAL;
        }
        if (nbytes == 0U) {
                return 0;
        }
        if (nbytes > rb->used) {
                return -EINVAL;
        }
        if (nbytes == rb->used) {
                rb->used = 0U;
                return 0;
        }
        memmove(rb->buf, rb->buf + nbytes, rb->used - nbytes);
        rb->used -= nbytes;
        return 0;
}

/// @brief Send all data in the buffer through the socket, handling partial sends and EINTR.
/// @param fd Socket file descriptor.
/// @param buf Buffer to send.
/// @param len Length of the buffer.
/// @return 0 on success, negative errno on failure.
static int ui_proto_send_all(int fd, const void *buf, size_t len)
{
        const unsigned char *p;
        size_t sent_total;

        if (fd < 0 || (!buf && len != 0U)) {
                return -EINVAL;
        }

        p = (const unsigned char *)buf;
        sent_total = 0U;

        while (sent_total < len) {
                ssize_t n = write(fd, p + sent_total, len - sent_total);
                if (n < 0) {
                        if (errno == EINTR) {
                                continue;
                        }
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                return -EAGAIN;
                        }
                        return -errno;
                }
                if (n == 0) {
                        return -EPIPE;
                }
                sent_total += (size_t)n;
        }
        return 0;
}

/// @brief Check if the given frame type is valid.
/// @param type Frame type to check.
/// @return 1 if the frame type is valid, 0 otherwise.
int ui_proto_is_valid_type(uint16_t type)
{
        switch ((ui_frame_type_t)type) {
        case UI_FRAME_HELLO:
        case UI_FRAME_HELLO_ACK:
        case UI_FRAME_PING:
        case UI_FRAME_PONG:
        case UI_FRAME_NOTIFY_SNAPSHOT:
        case UI_FRAME_LOG_CHUNK:
                return 1;
        default:
                return 0;
        }
}

/// @brief Validate the UI frame header.
/// @param hdr Pointer to the UI frame header to validate.
/// @return 0 if the header is valid, negative errno on failure.
int ui_proto_validate_hdr(const ui_frame_hdr_t *hdr)
{
        if (!hdr) {
                return -EINVAL;
        }
        if (hdr->magic != UI_FRAME_MAGIC) {
                return -EPROTO;
        }
        if (hdr->version != UI_PROTO_VERSION) {
                return -EPROTO;
        }
        if (!ui_proto_is_valid_type(hdr->type)) {
                return -EPROTO;
        }
        if (hdr->payload_len > UI_FRAME_PAYLOAD_MAX) {
                return -EMSGSIZE;
        }
        return 0;
}

/// @brief Initialize the UI RX buffer.
/// @param rb Pointer to the UI RX buffer to initialize.
void ui_rx_buf_init(ui_rx_buf_t *rb)
{
        if (!rb) {
                return;
        }
        memset(rb, 0, sizeof(*rb));
        rb->used = 0U;
}

/// @brief Append data to the UI RX buffer.
/// @param rb Pointer to the UI RX buffer.
/// @param data Pointer to the data to append.
/// @param len Length of the data to append.
/// @return 0 on success, negative errno on failure.
int ui_proto_append_rx(ui_rx_buf_t *rb, const void *data, size_t len)
{
        if (!rb || !data || len == 0) {
                return -EINVAL;
        }
        if ((rb->used + len) > sizeof(rb->buf)) {
                return -ENOSPC;
        }
        memcpy(rb->buf + rb->used, data, len);
        rb->used += len;
        return 0;
}

/// @brief Try to parse a complete UI frame from the RX buffer.
/// @param rb Pointer to the UI RX buffer.
/// @param out_hdr Pointer to the output frame header structure to fill.
/// @param out_payload Pointer to the output buffer to fill with the frame payload.
/// @param payload_cap Capacity of the output payload buffer.
/// @param out_payload_len Pointer to size_t to fill with the actual payload length of the parsed frame.
/// @return 0 on success, 1 if more data is needed, negative errno on failure.
int ui_proto_try_parse(ui_rx_buf_t *rb, ui_frame_hdr_t *out_hdr, unsigned char *out_payload, size_t payload_cap, size_t *out_payload_len)
{
        ui_frame_hdr_t hdr;
        size_t total_need;
        int rc;

        if (!rb || !out_hdr || !out_payload_len) {
                return -EINVAL;
        }

        if (rb->used < sizeof(ui_frame_hdr_t)) {
                return 1; /* more data needed */
        }

        memcpy(&hdr, rb->buf, sizeof(ui_frame_hdr_t));
        rc = ui_proto_validate_hdr(&hdr);
        if (rc != 0) {
                /* malformed header at buffer head */
                rb->used = 0U;
                return rc;
        }

        total_need = sizeof(ui_frame_hdr_t) + (size_t)hdr.payload_len;
        if (rb->used < total_need) {
                return 1; /* more data needed */
        }

        if ((size_t)hdr.payload_len > payload_cap) {
                return -EMSGSIZE;
        }
        *out_hdr = hdr;

        if (hdr.payload_len > 0U) {
                if (!out_payload) {
                        return -EINVAL;
                }
                memcpy(out_payload,
                       rb->buf + sizeof(ui_frame_hdr_t),
                       (size_t)hdr.payload_len);
        }
        *out_payload_len = (size_t)hdr.payload_len;

        rc = ui_proto_memmove_left(rb, total_need);
        if (rc != 0) {
                return rc;
        }
        return 0; /* success */
}

/// @brief Send a UI frame with the specified type, flags, and payload through the given socket file descriptor.
/// @param fd Socket file descriptor to send the frame through.
/// @param type Frame type (ui_frame_type_t).
/// @param flags Frame flags.
/// @param payload Pointer to the frame payload data (can be NULL if payload_len is 0).
/// @param payload_len Length of the frame payload data in bytes.
/// @return 0 on success, negative errno on failure.
int ui_proto_send_frame(int fd, uint16_t type, uint32_t flags, const void *payload, uint32_t payload_len)
{
        ui_frame_hdr_t hdr;
        int rc;

        if (fd < 0) {
                return -EINVAL;
        }
        if (payload_len > UI_FRAME_PAYLOAD_MAX) {
                return -EMSGSIZE;
        }
        if (payload_len > 0 && !payload) {
                return -EINVAL;
        }

        memset(&hdr, 0, sizeof(hdr));
        hdr.magic = UI_FRAME_MAGIC;
        hdr.version = UI_PROTO_VERSION;
        hdr.type = type;
        hdr.payload_len = payload_len;
        hdr.flags = flags;
        hdr.reserved = 0U;

        rc = ui_proto_validate_hdr(&hdr);
        if (rc != 0) {
                return rc;
        }

        rc = ui_proto_send_all(fd, &hdr, sizeof(hdr));
        if (rc != 0) {
                return rc;
        }

        if (payload_len > 0) {
                rc = ui_proto_send_all(fd, payload, payload_len);
                if (rc != 0) {
                        return rc;
                }
        }

        return 0;
}