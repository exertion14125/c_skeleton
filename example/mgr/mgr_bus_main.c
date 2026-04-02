#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "util/mgr_bus/mgr_bus.h"

#include "mgr/contract/mgr_addrs.h"

enum {
        CFG_CODE_NONE = 0,
        CFG_CODE_LOADED = 1,
        CFG_CODE_APPLIED = 2,
        CFG_CODE_FAILED = 3
};

enum {
        RED_CODE_NONE = 0,
        RED_CODE_ROLE_ACTIVE = 1,
        RED_CODE_ROLE_BACKUP = 2,
        RED_CODE_LINK_DOWN = 3
};

static const mgr_bus_addr_t g_mgr_bus_addrs[] = {
        APP_MGR_ADDR_SYS,
        APP_MGR_ADDR_CFG,
        APP_MGR_ADDR_GIO,
        APP_MGR_ADDR_RED,
        APP_MGR_ADDR_UI
};

mgr_bus_t* create_app_mgr_bus(void)
{
        mgr_bus_cfg_t cfg;

        memset(&cfg, 0, sizeof(cfg));
        cfg.addrs = g_mgr_bus_addrs;
        cfg.addr_count = (uint32_t)(sizeof(g_mgr_bus_addrs) / sizeof(g_mgr_bus_addrs[0]));
        cfg.qcap = 64;

        return mgr_bus_create(&cfg);
}

int cfg_mgr_notify_applied(mgr_bus_t *bus, uint64_t now_ms)
{
        return mgr_bus_send(bus, APP_MGR_ADDR_CFG, APP_MGR_ADDR_SYS, CFG_CODE_APPLIED, 0, 0, now_ms);
}

int cfg_mgr_notify_failed(mgr_bus_t *bus, int32_t err_code, uint64_t now_ms)
{
        return mgr_bus_send(bus, APP_MGR_ADDR_CFG, APP_MGR_ADDR_SYS, CFG_CODE_FAILED, err_code, 0, now_ms);
}

int red_mgr_notify_role_active(mgr_bus_t *bus, uint64_t now_ms)
{
        return mgr_bus_send(bus, APP_MGR_ADDR_RED, APP_MGR_ADDR_SYS, RED_CODE_ROLE_ACTIVE, 0, 0, now_ms);
}

int red_mgr_notify_role_backup(mgr_bus_t *bus, uint64_t now_ms)
{
        return mgr_bus_send(bus, APP_MGR_ADDR_RED, APP_MGR_ADDR_SYS, RED_CODE_ROLE_BACKUP, 0, 0, now_ms);
}

typedef struct sys_mgr_s sys_mgr_t;

enum {
        SYS_EVT_NONE = 0,
        SYS_EVT_CFG_APPLIED,
        SYS_EVT_CFG_FAILED,
        SYS_EVT_RED_ACTIVE,
        SYS_EVT_RED_BACKUP
};

static int sys_mgr_dispatch_event(sys_mgr_t *mgr, int evt, int32_t a, int32_t b)
{
        (void)mgr;
        (void)evt;
        (void)a;
        (void)b;
        /* 실제 구현에서는 dispatcher enqueue 또는 fsm event push */
        return 0;
}

int sys_mgr_pump_mgr_bus(sys_mgr_t *mgr, mgr_bus_t *bus, int32_t timeout_ms)
{
        mgr_bus_msg_t msg;
        int rc;

        if (!mgr || !bus) {
                return -1;
        }

        rc = mgr_bus_pop_for(bus, APP_MGR_ADDR_SYS, &msg, timeout_ms);
        if (rc != 1) {
                return rc; /* 0 timeout, -1 error/wakeup */
        }

        if (msg.src == APP_MGR_ADDR_CFG) {
                switch (msg.code) {
                case CFG_CODE_APPLIED:
                        return sys_mgr_dispatch_event(mgr, SYS_EVT_CFG_APPLIED, msg.a, msg.b);
                case CFG_CODE_FAILED:
                        return sys_mgr_dispatch_event(mgr, SYS_EVT_CFG_FAILED, msg.a, msg.b);
                default:
                        break;
                }
        } else if (msg.src == APP_MGR_ADDR_RED) {
                switch (msg.code) {
                case RED_CODE_ROLE_ACTIVE:
                        return sys_mgr_dispatch_event(mgr, SYS_EVT_RED_ACTIVE, msg.a, msg.b);
                case RED_CODE_ROLE_BACKUP:
                        return sys_mgr_dispatch_event(mgr, SYS_EVT_RED_BACKUP, msg.a, msg.b);
                default:
                        break;
                }
        }

        return 0;
}

typedef struct cfg_mgr_s cfg_mgr_t;

int cfg_mgr_pump_mgr_bus(cfg_mgr_t *mgr, mgr_bus_t *bus, int32_t timeout_ms)
{
        mgr_bus_msg_t msg;
        int rc;

        if (!mgr || !bus) {
                return -1;
        }

        rc = mgr_bus_pop_for(bus, APP_MGR_ADDR_CFG, &msg, timeout_ms);
        if (rc != 1) {
                return rc;
        }

        /*
         * cfg_mgr가 받아야 할 manager 협력 메시지가 있다면 여기서 처리.
         * 없으면 timeout 기반 idle loop만 유지 가능.
         */
        (void)msg;
        return 0;
}

static mgr_bus_t* create_example_bus(void)
{
        static const mgr_bus_addr_t addrs[] = {
                APP_MGR_ADDR_SYS,
                APP_MGR_ADDR_CFG,
                APP_MGR_ADDR_RED
        };
        mgr_bus_cfg_t cfg;

        memset(&cfg, 0, sizeof(cfg));
        cfg.addrs = addrs;
        cfg.addr_count = (uint32_t)(sizeof(addrs) / sizeof(addrs[0]));
        cfg.qcap = 8;

        return mgr_bus_create(&cfg);
}

int main(void)
{
        mgr_bus_t *bus;
        mgr_bus_msg_t msg;
        int rc;

        bus = create_example_bus();
        if (!bus) {
                printf("mgr_bus_create failed\n");
                return 1;
        }

        rc = mgr_bus_send(bus, APP_MGR_ADDR_CFG, APP_MGR_ADDR_SYS, CFG_CODE_APPLIED, 0, 0, 1000);
        printf("send cfg->sys rc=%d\n", rc);

        rc = mgr_bus_send(bus, APP_MGR_ADDR_RED, APP_MGR_ADDR_SYS, RED_CODE_ROLE_ACTIVE, 0, 0, 2000);
        printf("send red->sys rc=%d\n", rc);

        rc = mgr_bus_pop_for(bus, APP_MGR_ADDR_SYS, &msg, 0);
        if (rc == 1) {
                printf("sys pop #1: src=%s dst=%s code=%u ts=%llu\n",
                       app_mgr_addr_to_str(msg.src),
                       app_mgr_addr_to_str(msg.dst),
                       (unsigned)msg.code,
                       (unsigned long long)msg.ts_ms);
        }

        rc = mgr_bus_pop_for(bus, APP_MGR_ADDR_SYS, &msg, 0);
        if (rc == 1) {
                printf("sys pop #2: src=%s dst=%s code=%u ts=%llu\n",
                       app_mgr_addr_to_str(msg.src),
                       app_mgr_addr_to_str(msg.dst),
                       (unsigned)msg.code,
                       (unsigned long long)msg.ts_ms);
        }

        rc = mgr_bus_pop_for(bus, APP_MGR_ADDR_CFG, &msg, 0);
        printf("cfg pop rc=%d (expected 0)\n", rc);

        mgr_bus_destroy(&bus);
        return 0;
}