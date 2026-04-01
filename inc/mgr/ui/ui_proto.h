#ifndef __UI_PROTO_H__
#define __UI_PROTO_H__

#include <stddef.h>
#include <stdint.h>

#define UI_FRAME_MAGIC   0x55494D47u   ///< 'UIMG'
#define UI_PROTO_VERSION 1u

#define UI_RX_BUF_CAP    4096 ///< RX buffer capacity
#define UI_FRAME_PAYLOAD_MAX   1024  ///< Maximum frame payload length
#define UI_LOG_TEXT_MAX  256   ///< Maximum log text length

/// @brief UI frame types enumeration.
typedef enum ui_frame_type_e {
        UI_FRAME_HELLO = 1,     ///< Hello frame from UI to server, payload is ui_hello_payload_t
        UI_FRAME_HELLO_ACK = 2, ///< Hello ACK frame from server to UI, payload is ui_hello_payload_t

        UI_FRAME_PING = 10,     ///< Ping frame from server to UI, no payload
        UI_FRAME_PONG = 11,     ///< Pong frame from UI to server, no payload

        UI_FRAME_NOTIFY_SNAPSHOT = 20, ///< Notify snapshot ready frame from server to UI, payload is ui_notify_snapshot_payload_t

        UI_FRAME_LOG_CHUNK = 30 ///< Log chunk frame from server to UI, payload is ui_log_chunk_payload_t
} ui_frame_type_t;

/// @brief UI frame header structure.
typedef struct ui_frame_hdr_s {
        uint32_t magic;         ///< Magic number (UI_FRAME_MAGIC)
        uint16_t version;       ///< Protocol version (UI_PROTO_VERSION)
        uint16_t type;          ///< Frame type (ui_frame_type_t)
        uint32_t payload_len;   ///< Length of the payload
        uint32_t flags;         ///< Frame flags
        uint32_t reserved;      ///< Reserved for future use
} ui_frame_hdr_t;

/// @brief UI hello payload structure.
typedef struct ui_hello_payload_s {
        uint64_t last_seen_wseq; ///< Last seen write sequence
} ui_hello_payload_t;

/// @brief UI notify snapshot payload structure.
typedef struct ui_notify_snapshot_payload_s {
        uint16_t kind;           ///< Snapshot kind
        uint16_t reserved0;      ///< Reserved for future use
        uint32_t seq;            ///< Snapshot sequence
        uint32_t arg0;           ///< Argument 0
        uint32_t arg1;           ///< Argument 1
} ui_notify_snapshot_payload_t;

/// @brief UI log chunk payload structure.
typedef struct ui_log_chunk_payload_s {
        uint32_t seq;            ///< Log sequence
        uint16_t level;          ///< Log level
        uint16_t text_len;       ///< Length of the log text
        char text[UI_LOG_TEXT_MAX]; ///< Log text
} ui_log_chunk_payload_t;

/// @brief UI RX buffer structure.
typedef struct ui_rx_buf_s {
        unsigned char buf[UI_RX_BUF_CAP]; ///< RX buffer
        size_t used;                      ///< Used bytes in the buffer
} ui_rx_buf_t;

extern void ui_rx_buf_init(ui_rx_buf_t *rb);
extern int  ui_proto_append_rx(ui_rx_buf_t *rb, const void *data, size_t len);
extern int  ui_proto_try_parse(ui_rx_buf_t *rb, ui_frame_hdr_t *out_hdr, unsigned char *out_payload, size_t payload_cap, size_t *out_payload_len);
extern int  ui_proto_send_frame(int fd, uint16_t type, uint32_t flags, const void *payload, uint32_t payload_len);

#endif /* __UI_PROTO_H__ */