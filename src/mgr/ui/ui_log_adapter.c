#include <string.h>
#include <errno.h>

#include "mgr/ui/ui_proto.h"

/* util/log/internal/log_system.c 에서 extern 으로 참조 */
int ui_log_stream_frame_sender(void *user, int fd, const char *text, size_t text_len)
{
        ui_log_chunk_payload_t payload;

        (void)user;

        if (fd < 0 || !text) {
                return -EINVAL;
        }

        memset(&payload, 0, sizeof(payload));
        payload.seq = 0;
        payload.level = 0;

        if (text_len >= UI_LOG_TEXT_MAX) {
                text_len = UI_LOG_TEXT_MAX - 1;
        }

        payload.text_len = (uint16_t)text_len;
        memcpy(payload.text, text, text_len);
        payload.text[text_len] = '\0';

        return ui_proto_send_frame(fd, UI_FRAME_LOG_CHUNK, 0, &payload, sizeof(payload));
}