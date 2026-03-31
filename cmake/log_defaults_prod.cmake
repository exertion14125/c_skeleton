# cmake/log_defaults_prod.cmake

# Logger default values
set(LOG_MIN_LEVEL               "INFO" CACHE STRING "log min level")
set(LOG_QUEUE_CAP               1024   CACHE STRING "log queue capacity")
set(LOG_MAX_MSG                 4096   CACHE STRING "log max message bytes")

# file sink default values
set(LOG_FILE_PATH               "/tmp/@APP@_log.log" CACHE STRING "file sink path template")
set(LOG_FILE_MAX_FILES          1                    CACHE STRING "file max files")
set(LOG_FILE_MAX_SIZES          10240                CACHE STRING "file rotate size bytes")
set(LOG_FILE_FLUSH_LINES        1                    CACHE STRING "file flush lines")
set(LOG_FILE_FSYNC_LINES        0                    CACHE STRING "file fsync lines")
set(LOG_FILE_REMAIN_FIRST       1                    CACHE STRING "file remain_first")

# udp sink default values
set(LOG_UDP_IP                  "127.0.0.1" CACHE STRING "udp ip")
set(LOG_UDP_PORT                5140        CACHE STRING "udp port")

# ui sink default values
set(LOG_UI_ENABLE               1                       CACHE STRING "ui enable_default")
set(LOG_UI_UDS_PATH             "/var/run/@APP@.ui.uds" CACHE STRING "ui uds path template")
set(LOG_UI_UDS_NOTIFY           1                       CACHE STRING "ui uds notify")
set(LOG_UI_ENABLE_NO_UI         1                       CACHE STRING "ui enable when no ui")