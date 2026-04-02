#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include "util/mgr_bus/mgr_bus.h"

/// @brief Inbox structure for manager bus, containing address, message queue, capacity, read/write indices, and message count.
typedef struct inbox_s {
        mgr_bus_addr_t addr;
        mgr_bus_msg_t *q;
        uint32_t cap;
        uint32_t r;
        uint32_t w;
        uint32_t n;
} inbox_t;

/// @brief Manager bus structure containing mutex, condition variable, wake flag, and inboxes for message queues.
struct mgr_bus_s {
        pthread_mutex_t mt;
        pthread_cond_t  cv;

        int wake;
        inbox_t *inbox;
        uint32_t inbox_count;
        
};

/// @brief Add milliseconds to a timespec structure, handling overflow of nanoseconds into seconds.
/// @param ts Timespec structure to modify
/// @param ms Milliseconds to add
static void ts_add_ms(struct timespec *ts, int32_t ms)
{
        /* CLOCK_REALTIME 기반 timedwait (Linux 2.6) */
        ts->tv_sec  += (time_t)(ms / 1000);
        ts->tv_nsec += (long)(ms % 1000) * 1000000L;
        if (ts->tv_nsec >= 1000000000L) {
                ts->tv_sec += 1;
                ts->tv_nsec -= 1000000000L;
        }
}

/// @brief Find the index of the inbox corresponding to the given destination address in the manager bus.
/// @param b Pointer to the manager bus structure
/// @param dst Destination address to find
/// @return Index of the inbox if found, or -1 if not found or on error
static int find_inbox_index(const mgr_bus_t *b, mgr_bus_addr_t dst)
{
        uint32_t i;
        if (!b || !b->inbox || b->inbox_count == 0) {
                return -1;
        }
        for (i = 0; i < b->inbox_count; ++i) {
                if (b->inbox[i].addr == dst) {
                        return (int)i;
                }
        }
        return -1;
}

/// @brief Try to push a message into the inbox corresponding to the message's destination address. For broadcast messages, try to push into all inboxes.
/// @param ib Pointer to the manager bus structure
/// @param m Pointer to the message structure containing the destination address
/// @return 1 if the destination address is valid, 0 otherwise
static int inbox_try_push(inbox_t *ib, const mgr_bus_msg_t *m)
{
        if (!ib || !ib->q || !m) {
                return -1;
        }
        if (ib->n >= ib->cap) {
                return -1;
        }
        ib->q[ib->w] = *m;
        ib->w = (ib->w + 1U) % ib->cap;
        ib->n++;
        return 0;
}

/// @brief Try to pop a message from the inbox. If the inbox is empty, return 0. If a message is successfully popped, return 1. On error, return -1.
/// @param ib Pointer to the inbox structure
/// @param out Pointer to the message structure where the popped message will be stored
/// @return 1 if a message is popped, 0 if the inbox is empty, -1 on error
static int inbox_try_pop(inbox_t *ib, mgr_bus_msg_t *out)
{
        if (!ib || !ib->q || !out) {
                return -1;
        }
        if (ib->n == 0) {
                return 0;
        }
        *out = ib->q[ib->r];
        ib->r = (ib->r + 1U) % ib->cap;
        ib->n--;
        return 1;
}

/// @brief Create a manager bus with the specified configuration, including inbox addresses and queue capacity. Returns a pointer to the created manager bus structure, or NULL on failure.
/// @param cfg Pointer to the configuration structure containing inbox addresses, count, and queue capacity
/// @return Pointer to the created manager bus structure, or NULL on failure
mgr_bus_t* mgr_bus_create(const mgr_bus_cfg_t *cfg)
{
        mgr_bus_t *b;
        uint32_t qcap;
        if (!cfg || !cfg->addrs || cfg->addr_count == 0) {
                return NULL;
        }

        qcap = (cfg->qcap == 0) ? 64U : cfg->qcap;

        b = (mgr_bus_t*)calloc(1, sizeof(*b));
        if (!b) {
                return NULL;
        }

        b->inbox = (inbox_t*)calloc(cfg->addr_count, sizeof(inbox_t));
        if (!b->inbox) {
                free(b);
                return NULL;
        }
        b->inbox_count = cfg->addr_count;

        for (uint32_t i = 0; i < b->inbox_count; i++) {
                b->inbox[i].addr = cfg->addrs[i];
                b->inbox[i].q = (mgr_bus_msg_t*)calloc(qcap, sizeof(mgr_bus_msg_t));
                if (!b->inbox[i].q) {
                        for (uint32_t j = 0; j < b->inbox_count; j++) {
                                free(b->inbox[j].q);
                                b->inbox[j].q = NULL;
                        }
                        free(b->inbox);
                        free(b);
                        return NULL;
                }
                b->inbox[i].cap = qcap;
        }

        pthread_mutex_init(&b->mt, NULL);
        pthread_cond_init(&b->cv, NULL);
        return b;
}

/// @brief Destroy the manager bus and free all associated resources. After calling this function, the pointer to the manager bus will be set to NULL.
/// @param pb Pointer to the pointer of the manager bus structure to be destroyed
void mgr_bus_destroy(mgr_bus_t **pb)
{
        mgr_bus_t *b;
        if (!pb || !*pb) return;
        b = *pb;
        *pb = NULL;

        pthread_mutex_lock(&b->mt);
        b->wake = 1;
        pthread_cond_broadcast(&b->cv);
        pthread_mutex_unlock(&b->mt);

        pthread_cond_destroy(&b->cv);
        pthread_mutex_destroy(&b->mt);

        if (b->inbox) {
                for (uint32_t i = 0; i < b->inbox_count; ++i) {
                        free(b->inbox[i].q);
                        b->inbox[i].q = NULL;
                }
                free(b->inbox);
                b->inbox = NULL;
        }
        b->inbox_count = 0;
        
        free(b);
}

///// @brief Wake up all threads waiting on the manager bus condition variable, typically used to signal shutdown. 
///          After calling this function, all waiting threads will be unblocked and can check the wake flag to determine if they should exit.
/// @param b Pointer to the manager bus structure
void mgr_bus_wakeup(mgr_bus_t *b)
{
        if (!b) return;
        pthread_mutex_lock(&b->mt);
        b->wake = 1;
        pthread_cond_broadcast(&b->cv);
        pthread_mutex_unlock(&b->mt);
}

/// @brief Try to push a message into the manager bus. 
/// @details If the message's destination address is valid, it will be pushed into the corresponding inbox. 
///          For broadcast messages, it will be pushed into all inboxes. 
///          Returns 0 on success, or -1 if the destination address is invalid or if the inbox is full.
/// @param b Pointer to the manager bus structure
/// @param m Pointer to the message structure to be pushed
/// @return 0 on success, -1 on failure
int mgr_bus_try_push(mgr_bus_t *b, const mgr_bus_msg_t *m)
{
        int rc = 0;

        if (!b || !m) return -1;

        pthread_mutex_lock(&b->mt);
        if (m->dst == MGR_BUS_ADDR_BROADCAST) {
                int any_ok = 0;
                int any_drop = 0;
                for (uint32_t i = 0; i < b->inbox_count; ++i) {
                        int prc = inbox_try_push(&b->inbox[i], m);
                        if (prc == 0) {
                                any_ok = 1;
                        } else {
                                any_drop = 1;
                        }
                }
                if (!any_ok) {
                        rc = -1;
                } else {
                        rc = (any_drop ? -1 : 0);
                }
                pthread_cond_broadcast(&b->cv);
        } else {
                int idx = find_inbox_index(b, m->dst);
                if (idx < 0) {
                        rc = -1;
                } else {
                        rc = inbox_try_push(&b->inbox[idx], m);
                        if (rc == 0) {
                                pthread_cond_broadcast(&b->cv);
                        } else {
                                rc = -1;
                        }
                }
        }
        pthread_mutex_unlock(&b->mt);
        return rc;
}

/// @brief Pop a message from any inbox in the manager bus. The function will block until a message is available or until the specified timeout expires.
/// @param b Pointer to the manager bus structure
/// @param out Pointer to the message structure where the popped message will be stored
/// @param timeout_ms Timeout in milliseconds (negative for infinite wait, zero for non-blocking, positive for timed wait)
/// @return 1 if a message is popped, 0 if timeout or empty, -1 on error or wakeup
int mgr_bus_pop(mgr_bus_t *b, mgr_bus_msg_t *out, int32_t timeout_ms)
{
        if (!b || !out) return -1;
        for (;;) {
                pthread_mutex_lock(&b->mt);
                while (!b->wake) {
                        int found = 0;
                        for (uint32_t i = 0; i < b->inbox_count; i++) {
                                if (b->inbox[i].n > 0) {
                                        found = 1;
                                        break;
                                }
                        }
                        if (found) {
                                break;
                        }

                        if (timeout_ms == 0) { 
                                pthread_mutex_unlock(&b->mt); 
                                return 0; 
                        }
                        if (timeout_ms < 0) {
                                pthread_cond_wait(&b->cv, &b->mt);
                        } else {
                                struct timespec ts;
                                clock_gettime(CLOCK_REALTIME, &ts);
                                ts_add_ms(&ts, timeout_ms);
                                int wrc = pthread_cond_timedwait(&b->cv, &b->mt, &ts);
                                if (wrc == ETIMEDOUT) { 
                                        pthread_mutex_unlock(&b->mt); 
                                        return 0; 
                                }
                        }
                }
                if (b->wake) { 
                        pthread_mutex_unlock(&b->mt); 
                        return -1; 
                }

                for (uint32_t i = 0; i < b->inbox_count; i++) {
                        if (b->inbox[i].n > 0) {
                                int rc = inbox_try_pop(&b->inbox[i], out);
                                pthread_mutex_unlock(&b->mt);
                                return (rc == 1) ? 1 : -1;
                        }
                }
                pthread_mutex_unlock(&b->mt);
                /* loop again (should be rare) */
        }
}

/// @brief Pop a message from the manager bus for a specific destination address.
///        The function will block until a message is available or until the specified timeout expires.
/// @param b Pointer to the manager bus structure
/// @param dst Destination address for which to pop messages
/// @param out Pointer to the message structure where the popped message will be stored
/// @param timeout_ms Timeout in milliseconds (negative for infinite wait, zero for non-blocking, positive for timed wait)
/// @return 1 if a message is popped, 0 if timeout or empty, -1 on error or wakeup
int mgr_bus_pop_for(mgr_bus_t *b, mgr_bus_addr_t dst, mgr_bus_msg_t *out, int32_t timeout_ms)
{
        int rc = -1;
        int idx;
        if (!b || !out) {
                return -1;
        }
        idx = find_inbox_index(b, dst);
        if (idx < 0) {
                return -1;
        }

        pthread_mutex_lock(&b->mt);

        while (b->inbox[idx].n == 0 && !b->wake) {

                if (timeout_ms == 0) {
                        pthread_mutex_unlock(&b->mt);
                        return 0;
                }
                if (timeout_ms < 0) {
                        pthread_cond_wait(&b->cv, &b->mt);
                } else {
                        struct timespec ts;
                        clock_gettime(CLOCK_REALTIME, &ts);
                        ts_add_ms(&ts, timeout_ms);
                        {
                                int wrc = pthread_cond_timedwait(&b->cv, &b->mt, &ts);
                                if (wrc == ETIMEDOUT) {
                                        pthread_mutex_unlock(&b->mt);
                                        return 0;
                                }
                        }
                }
        }

        if (b->wake) {
                pthread_mutex_unlock(&b->mt);
                return -1;
        }

        rc = inbox_try_pop(&b->inbox[idx], out);
        if (rc < 0) rc = -1;
        rc = 1;
        pthread_mutex_unlock(&b->mt);
        return rc;
}

/// @brief Send a message through the manager bus by specifying source address, destination address, message code, parameters, and timeout.
/// @param bus Pointer to the manager bus structure
/// @param src Source address of the message
/// @param dst Destination address of the message
/// @param code Domain-defined message code
/// @param a First integer parameter
/// @param b Second integer parameter
/// @param ts_ms Timestamp in milliseconds for the message
/// @return 0 on success, -1 on failure (e.g., invalid destination address or full inbox)
int mgr_bus_send(mgr_bus_t *bus, mgr_bus_addr_t src, mgr_bus_addr_t dst, uint16_t code, int32_t a, int32_t b, uint64_t ts_ms)
{
        mgr_bus_msg_t msg;

        if (!bus) {
                return -1;
        }

        memset(&msg, 0, sizeof(msg));
        msg.src = src;
        msg.dst = dst;
        msg.code = code;
        msg.a = a;
        msg.b = b;
        msg.ts_ms = ts_ms;

        return mgr_bus_try_push(bus, &msg);
}