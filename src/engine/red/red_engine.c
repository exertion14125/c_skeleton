#include <stdlib.h>
#include <string.h>

#include "engine/red/red_engine_priv.h"

red_engine_t *alloc_red_engine(void)
{
        red_engine_t *e = (red_engine_t *)calloc(1, sizeof(*e));
        return e;
}

void destroy_red_engine(red_engine_t **pe)
{
        red_engine_t *e;

        if (!pe || !*pe) {
                return;
        }

        e = *pe;
        *pe = NULL;

        deinit_red_engine(e);
        free(e);
}

int init_red_engine(red_engine_t *e)
{
        red_policy_cfg_t pcfg;

        if (!e) {
                return -1;
        }

        memset(e, 0, sizeof(*e));

        memset(&pcfg, 0, sizeof(pcfg));
        pcfg.fail_threshold = 3U;
        pcfg.recover_threshold = 2U;

        if (red_policy_init(&e->policy, &pcfg) != 0) {
                return -1;
        }

        e->last_proposed_decision = RED_DECISION_NONE;
        return 0;
}

void deinit_red_engine(red_engine_t *e)
{
        if (!e) {
                return;
        }

        /* 현재 동적 하위 자원 없음 */
}

int red_engine_apply(red_engine_t *e,
                     const red_eng_input_t *in,
                     red_eng_output_t *out)
{
        red_decision_t dec = RED_DECISION_NONE;

        if (!e || !in || !out) {
                return -1;
        }

        memset(out, 0, sizeof(*out));
        out->req_id = in->req_id;

        switch (in->kind) {
        case RED_ENG_IN_EVAL_REQ:
                if (in->arg0 < 0) {
                        red_policy_feed_error(&e->policy);
                        e->peer_err_count++;
                } else {
                        red_policy_feed_ok(&e->policy);
                        e->peer_ok_count++;
                }

                if (red_policy_eval(&e->policy, &dec) != 0) {
                        out->action = RED_ENG_ACT_NONE;
                        out->value = (int32_t)RED_DECISION_NONE;
                        out->reason_code = 1U;
                        return 0;
                }

                switch (dec) {
                case RED_DECISION_FAILOVER:
                        out->action = RED_ENG_ACT_PROPOSE_FAILOVER;
                        break;

                case RED_DECISION_RECOVER:
                        out->action = RED_ENG_ACT_PROPOSE_RECOVER;
                        break;

                case RED_DECISION_KEEP:
                        out->action = RED_ENG_ACT_KEEP;
                        break;

                case RED_DECISION_NONE:
                default:
                        out->action = RED_ENG_ACT_NONE;
                        break;
                }

                out->value = (int32_t)dec;
                return 0;

        case RED_ENG_IN_PEER_OK:
                red_policy_feed_ok(&e->policy);
                e->peer_ok_count++;
                out->action = RED_ENG_ACT_KEEP;
                out->value = (int32_t)RED_DECISION_KEEP;
                e->last_proposed_decision = RED_DECISION_KEEP;
                return 0;

        case RED_ENG_IN_PEER_ERR:
                red_policy_feed_error(&e->policy);
                e->peer_err_count++;
                out->action = RED_ENG_ACT_KEEP;
                out->value = (int32_t)RED_DECISION_KEEP;
                return 0;

        case RED_ENG_IN_TIMEOUT:
                e->timeout_count++;
                red_policy_feed_error(&e->policy);
                out->action = RED_ENG_ACT_KEEP;
                out->value = (int32_t)RED_DECISION_KEEP;
                return 0;

        case RED_ENG_IN_OPERATOR_HOLD:
                e->hold_flag = 1U;
                out->action = RED_ENG_ACT_PROPOSE_HOLD;
                out->value = (int32_t)RED_DECISION_KEEP;
                return 0;

        case RED_ENG_IN_OPERATOR_RELEASE:
                e->hold_flag = 0U;
                out->action = RED_ENG_ACT_KEEP;
                out->value = (int32_t)RED_DECISION_KEEP;
                e->last_proposed_decision = RED_DECISION_KEEP;
                return 0;

        case RED_ENG_IN_TICK_EVAL:
                if (e->hold_flag) {
                        if (e->last_proposed_decision == RED_DECISION_KEEP) {
                                out->action = RED_ENG_ACT_KEEP;
                                out->value = (int32_t)RED_DECISION_KEEP;
                                return 0;
                        }

                        out->action = RED_ENG_ACT_PROPOSE_HOLD;
                        out->value = (int32_t)RED_DECISION_KEEP;
                        e->last_proposed_decision = RED_DECISION_KEEP;
                        return 0;
                }

                if (red_policy_eval(&e->policy, &dec) != 0) {
                        out->action = RED_ENG_ACT_NONE;
                        out->value = (int32_t)RED_DECISION_NONE;
                        out->reason_code = 2U;
                        return 0;
                }

                if (dec == e->last_proposed_decision) {
                        out->action = RED_ENG_ACT_KEEP;
                        out->value = (int32_t)dec;
                        return 0;
                }

                switch (dec) {
                case RED_DECISION_FAILOVER:
                        out->action = RED_ENG_ACT_PROPOSE_FAILOVER;
                        e->last_proposed_decision = dec;
                        break;

                case RED_DECISION_RECOVER:
                        out->action = RED_ENG_ACT_PROPOSE_RECOVER;
                        e->last_proposed_decision = dec;
                        break;

                case RED_DECISION_KEEP:
                        out->action = RED_ENG_ACT_KEEP;
                        e->last_proposed_decision = dec;
                        break;

                case RED_DECISION_NONE:
                default:
                        out->action = RED_ENG_ACT_NONE;
                        break;
                }

                out->value = (int32_t)dec;
                return 0;

        case RED_ENG_IN_NONE:
        default:
                return -1;
        }
}