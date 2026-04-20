#ifndef __MGR_RED_RED_POLICY_H__
#define __MGR_RED_RED_POLICY_H__

#include <stdint.h>

typedef enum red_decision_e {
        RED_DECISION_NONE = 0,
        RED_DECISION_KEEP = 1,
        RED_DECISION_FAILOVER = 2,
        RED_DECISION_RECOVER = 3
} red_decision_t;

typedef struct red_policy_cfg_s {
        uint32_t fail_threshold;
        uint32_t recover_threshold;
} red_policy_cfg_t;

typedef struct red_policy_s {
        red_policy_cfg_t cfg;
        uint32_t err_score;
        uint32_t ok_score;
} red_policy_t;

extern int red_policy_init(red_policy_t *p, const red_policy_cfg_t *cfg);
extern void red_policy_feed_error(red_policy_t *p);
extern void red_policy_feed_ok(red_policy_t *p);
extern int red_policy_eval(red_policy_t *p, red_decision_t *out);

#endif /* __MGR_RED_RED_POLICY_H__ */