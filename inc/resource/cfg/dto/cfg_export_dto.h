#ifndef __RESOURCE_CFG_DTO_CFG_EXPORT_DTO_H__
#define __RESOURCE_CFG_DTO_CFG_EXPORT_DTO_H__

#include <stdint.h>

typedef struct cfg_export_dto_s {
        uint32_t cfg_generation;
        uint32_t scan_period_ms;
        uint32_t card_count;
        uint32_t total_channel_count;
        uint32_t flags;
} cfg_export_dto_t;

#endif /* __RESOURCE_CFG_DTO_CFG_EXPORT_DTO_H__ */