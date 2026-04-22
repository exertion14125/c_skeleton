#ifndef __RESOURCE_CFG_CFG_REQUEST_DTO_H__
#define __RESOURCE_CFG_CFG_REQUEST_DTO_H__

#include <stdint.h>

typedef enum cfg_request_kind_e {
        CFG_REQ_KIND_NONE = 0,
        CFG_REQ_KIND_OPEN = 1,
        CFG_REQ_KIND_ADJUST = 2,
        CFG_REQ_KIND_REOPEN = 3,
        CFG_REQ_KIND_MODIFY = 4
} cfg_request_kind_t;

typedef struct cfg_modify_request_s {
        uint32_t out_card_no;
        uint32_t out_card_type;
        uint32_t out_ch0;
        uint32_t out_ch1;
} cfg_modify_request_t;

typedef struct cfg_request_dto_s {
        uint32_t version;
        uint32_t valid;
        uint64_t ts_ms;

        uint32_t req_kind;
        int32_t arg0;
        int32_t arg1;

        cfg_modify_request_t modify;
} cfg_request_dto_t;

#endif /* __RESOURCE_CFG_CFG_REQUEST_DTO_H__ */