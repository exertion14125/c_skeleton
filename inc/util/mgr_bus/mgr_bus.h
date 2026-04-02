///@file mgr_bus.h
///@brief Manager bus for inter-manager communication. 
///       Provides thread-safe message passing with multiple inboxes and support for broadcast messages. See mgr_bus.c for implementation details.

#ifndef __UTIL_MGR_BUS_H__
#define __UTIL_MGR_BUS_H__

#include <stdint.h>

typedef uint32_t mgr_bus_addr_t;
#define MGR_BUS_ADDR_BROADCAST ((mgr_bus_addr_t)0xFFFFFFFFu)

typedef struct mgr_bus_s mgr_bus_t;

/// @brief Message structure for manager bus communication
typedef struct mgr_bus_msg_s {
        mgr_bus_addr_t dst; ///< project-defined destination address
        mgr_bus_addr_t src; ///< project-defined source address
        uint16_t code;    ///< domain-defined message code
        uint16_t flags;   ///< reserved
        int32_t  a;
        int32_t  b;
        uint64_t ts_ms;   ///< optional: producer timestamp
} mgr_bus_msg_t;

/// @brief Configuration structure for manager bus creation
typedef struct mgr_bus_cfg_s {
        const mgr_bus_addr_t *addrs; ///< inbox address table
        uint32_t addr_count;         ///< number of inbox addresses
        uint32_t qcap;               ///< per-inbox queue capacity
} mgr_bus_cfg_t;

//===== Lifecycle =====//
extern mgr_bus_t* mgr_bus_create(const mgr_bus_cfg_t *cfg);
extern void mgr_bus_destroy(mgr_bus_t **pb);
extern void mgr_bus_wakeup(mgr_bus_t *b);

/// non-blocking push: 0 ok, -1 full
extern int mgr_bus_try_push(mgr_bus_t *b, const mgr_bus_msg_t *m);

/// pop with timeout:
///  - timeout_ms < 0 : wait forever
///  - timeout_ms = 0 : non-blocking
///  - timeout_ms > 0 : timed wait
/// return: 1 popped, 0 timeout, -1 error
/// legacy API: pop from any inbox. Not recommended for "mgr_bus_pop_for".
extern int mgr_bus_pop(mgr_bus_t *b, mgr_bus_msg_t *out, int32_t timeout_ms);

/// pop only messages for dst inbox.
/// IMPORTANT: Manager must only pop messages for its own dst inbox.
/// return: 1 popped, 0 timeout/empty, -1 error/wakeup
extern int mgr_bus_pop_for(mgr_bus_t *b, mgr_bus_addr_t dst, mgr_bus_msg_t *out, int32_t timeout_ms);

extern int mgr_bus_send(mgr_bus_t *bus, mgr_bus_addr_t src, mgr_bus_addr_t dst, uint16_t code, int32_t a, int32_t b, uint64_t ts_ms);
#endif /* __UTIL_MGR_BUS_H__ */
