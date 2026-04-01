#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "mgr/ui/ui_proto.h"

/// @brief Send all data in the buffer through the socket, handling partial sends and EINTR.
/// @param fd Socket file descriptor.
/// @param buf Buffer to send.
/// @param len Length of the buffer.
/// @return 0 on success, negative errno on failure.
static int send_all(int fd, const void *buf, size_t len)
{
        size_t sent = 0;
        const unsigned char *p = (const unsigned char *)buf;
        while (sent < len) {
                ssize_t n = send(fd, p + sent, len - sent, 0);
                if (n < 0) {
                        if (errno == EINTR) {
                                continue;
                        }
                        return -errno;
                }
                if (n == 0) {
                        return -EPIPE;
                }
                sent += (size_t)n;
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
        size_t remain;
        size_t frame_len;
        ui_frame_hdr_t hdr;

        if (!rb || !out_hdr || !out_payload || !out_payload_len) {
                return -EINVAL;
        }
        if (rb->used < sizeof(ui_frame_hdr_t)) {
                return 1; /* need more */
        }
        memcpy(&hdr, rb->buf, sizeof(hdr));

        if (hdr.magic != UI_FRAME_MAGIC) {
                return -EPROTO;
        }
        if (hdr.version != UI_PROTO_VERSION) {
                return -EPROTO;
        }
        if (hdr.payload_len > payload_cap) {
                return -EMSGSIZE;
        }

        frame_len = sizeof(ui_frame_hdr_t) + (size_t)hdr.payload_len;
        if (rb->used < frame_len) {
                return 1; /* need more */
        }

        memcpy(out_hdr, &hdr, sizeof(hdr));
        if (hdr.payload_len > 0) {
                memcpy(out_payload, rb->buf + sizeof(ui_frame_hdr_t), hdr.payload_len);
        }
        *out_payload_len = hdr.payload_len;

        remain = rb->used - frame_len;
        if (remain > 0) {
                memmove(rb->buf, rb->buf + frame_len, remain);
        }
        rb->used = remain;

        return 0;
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
        if (payload_len > 0 && !payload) {
                return -EINVAL;
        }

        memset(&hdr, 0, sizeof(hdr));
        hdr.magic = UI_FRAME_MAGIC;
        hdr.version = UI_PROTO_VERSION;
        hdr.type = type;
        hdr.payload_len = payload_len;
        hdr.flags = flags;

        rc = send_all(fd, &hdr, sizeof(hdr));
        if (rc != 0) {
                return rc;
        }

        if (payload_len > 0) {
                rc = send_all(fd, payload, payload_len);
                if (rc != 0) {
                        return rc;
                }
        }

        return 0;
}