#include <stdio.h>
#include <error.h>

#include "skeleton_const.h"

#include "util/log/log_bootstrap.h"

/// @brief Bootstrap log system.
/// @param void
/// @return 0 on success, negative errno on failure.
int bootstrap_log_system(void)
{
        if (init_log_system() != 0) {
                fprintf(stderr, "init_log_system failed\n");
                return -1;
        }
        int rc = log_system_reload_from_source(&(log_cfg_src_t) {
                .kind = LOG_CFG_SRC_INI_PATH,
                .u.path = DEF_LOG_INI_PATH APP_EXEC_NAME "_log.ini" // /data/conf/log/lcp_proc_log.ini
        });
        if (rc < 0) {
                fprintf(stderr, "log_system_reload_from_source failed, rc=%d\n", rc);
                return rc;
        }
        log_cfg_out_t lco;
        get_log_system_cfg(&lco); // For debug purpose.
        LOGI_T("LOG", "Log system initialized. Level=%d, File=%s, UDP=%s:%d, UI=%s\n",
             lco.level,
             lco.file_enable ? lco.file_fpath : "Disabled",
             lco.udp_enable ? lco.udp_ip : "Disabled",
             lco.udp_enable ? lco.udp_port : 0,
             lco.ui_enable ? "Enabled" : "Disabled");
        return 0;
}