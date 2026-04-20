#include <string.h>
#include <errno.h>

#include "mgr/red/red_policy.h"

int red_policy_init(red_policy_t *p, const red_policy_cfg_t *cfg)
{
        if (!p) {
                return -EINVAL;
        }

        memset(p, 0, sizeof(*p));
        if (cfg) {
                p->cfg = *cfg;
        }
        if (p->cfg.fail_threshold == 0U) {
                p->cfg.fail_threshold = 3U;
        }
        if (p->cfg.recover_threshold == 0U) {
                p->cfg.recover_threshold = 2U;
        }

        return 0;
}

void red_policy_feed_error(red_policy_t *p)
{
        if (!p) {
                return;
        }
        p->err_score++;
        p->ok_score = 0;
}

void red_policy_feed_ok(red_policy_t *p)
{
        if (!p) {
                return;
        }
        p->ok_score++;
        if (p->err_score > 0U) {
                p->err_score--;
        }
}

int red_policy_eval(red_policy_t *p, red_decision_t *out)
{
        if (!p || !out) {
                return -EINVAL;
        }

        *out = RED_DECISION_KEEP;
        if (p->err_score >= p->cfg.fail_threshold) {
                *out = RED_DECISION_FAILOVER;
        } else if (p->ok_score >= p->cfg.recover_threshold) {
                *out = RED_DECISION_RECOVER;
        }

        return 0;
}