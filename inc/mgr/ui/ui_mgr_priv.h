#ifndef __UI_MGR_PRIV_H__
#define __UI_MGR_PRIV_H__

#include <pthread.h>
#include "mgr/ui/ui_proto.h"
#include "mgr/ui/ui_mgr.h"


typedef struct ra_ui_uds_srv_s ra_ui_uds_srv_t;

#define UI_HELLO_SIZE (4 + (int)sizeof(uint64_t))

/// @brief UI manager context structure.
struct ui_mgr_s {
        ui_mgr_cfg_t cfg; ///< Configuration
        ui_mgr_cb_t cb;   ///< Callbacks

        ra_ui_uds_srv_t *srv; ///< UI UDS server

        ui_mgr_state_t state;   ///< Current state
        
        uint64_t last_rx_ms;  ///< Last received time in milliseconds (stale policy)
        uint64_t last_tx_ms;  ///< Last transmitted time in milliseconds (ping/server tx monitoring)
        uint64_t last_ping_ms; ///< Next ping due time in milliseconds 
                               /// last_ping_ms is treated as "next ping due time (ms)" (not last attempt time).

        uint64_t attached_ms; ///< Last attached time in milliseconds

        int      await_pong;       ///< 1 if ping sent and waiting for PONG
        uint64_t pong_deadline_ms; ///< PONG deadline time in milliseconds

        uint64_t hello_last_seen_wseq; ///< Last seen wseq in hello message

        pthread_t runloop_tid;
        int runloop_created;
        volatile int runloop_run;
        volatile int start_req;
        volatile int started;

        ui_rx_buf_t rx_buf;
};

#endif /* __UI_MGR_PRIV_H__ */