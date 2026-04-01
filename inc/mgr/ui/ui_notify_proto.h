#ifndef __UI_NOTIFY_PROTO_H__
#define __UI_NOTIFY_PROTO_H__

#include <stdint.h>

typedef enum {
        UI_MSG_NOP = 0,
        UI_MSG_NOTIFY_SNAPSHOT_READY = 1
} ui_msg_type_t;

typedef enum {
        UI_SNAP_KIND_MAIN = 0,
        UI_SNAP_KIND_KPI  = 1,
} ui_snapshot_kind_t;

typedef struct ui_notify_msg_s {
        uint16_t type;   /* ui_msg_type_t */
        uint16_t kind;   /* ui_snapshot_kind_t */
        uint32_t seq;    /* SHM snapshot commit seq */
        uint32_t arg0;
        uint32_t arg1;
} ui_notify_msg_t;

#endif /* __UI_NOTIFY_PROTO_H__ */