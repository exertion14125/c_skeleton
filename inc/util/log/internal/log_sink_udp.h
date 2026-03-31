#ifndef __LOG_SINK_UDP_H__
#define __LOG_SINK_UDP_H__

#include "log_sink.h"

#define LOG_SINK_UDP_CFG_DEF_IP   "127.0.0.1"
#define LOG_SINK_UDP_CFG_DEF_PORT (8080)

#define LOG_SINK_UDP_CTX_Q_CAP (256)

/// @brief Log sink UDP configuration structure.
struct log_sink_udp_cfg_s {
        char host[64]; //destination host IP
        unsigned short port; //destination port
        unsigned q_cap; //UDP send queue capacity
};
typedef struct log_sink_udp_cfg_s log_sink_udp_cfg_t;

extern int log_sink_udp_reconfigure(log_sink_t *s, const log_sink_udp_cfg_t *cfg); //runtime reconfigure: updates destination host/port safely.
extern log_sink_t* log_sink_udp_create(const log_sink_udp_cfg_t *cfg);

#endif /* _LOG_SINK_UDP_H_ */
