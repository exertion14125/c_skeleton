#ifndef __MGR_CONTRACT_MGR_ADDRS_H__
#define __MGR_CONTRACT_MGR_ADDRS_H__

#include <stdint.h>
#include "util/mgr_bus/mgr_bus.h"

/// Manager addr. each project should define its own addr values, and util/mgr_bus does not know the meaning of these values.
enum {
        APP_MGR_ADDR_NONE = 0,

        APP_MGR_ADDR_SYS = 1,
        APP_MGR_ADDR_CFG = 2,
        APP_MGR_ADDR_GIO = 3,
        APP_MGR_ADDR_RED = 4,
        APP_MGR_ADDR_LOGIC = 5,
        APP_MGR_ADDR_UI  = 6,

        APP_MGR_ADDR_MAX
};

/// @brief Convert manager bus address to string for debugging purposes. 
/// This function maps known manager addresses to human-readable strings, and returns "unknown" for any unrecognized addresses.
/// @param addr The manager bus address to convert
/// @return A string representation of the manager bus address
static inline const char* app_mgr_addr_to_str(mgr_bus_addr_t addr)
{
        switch (addr) {
        case APP_MGR_ADDR_SYS: return "sys";
        case APP_MGR_ADDR_CFG: return "cfg";
        case APP_MGR_ADDR_GIO: return "gio";
        case APP_MGR_ADDR_RED: return "red";
        case APP_MGR_ADDR_LOGIC: return "logic";
        case APP_MGR_ADDR_UI:  return "ui";
        default:               return "unknown";
        }
}
#endif /* __MGR_CONTRACT_MGR_ADDRS_H__ */