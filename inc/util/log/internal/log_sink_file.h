#ifndef __LOG_SINK_FILE_H__
#define __LOG_SINK_FILE_H__

#include "log_sink.h"

#define LOG_SINK_FILE_NAME_PREFIX APP_EXEC_NAME".log"
#define LOG_SINK_FILE_CTX_MAX_PATH (256)
#define LOG_SINK_FILE_CFG_MAX_FILE (3)

/// @brief Log sink file configuration structure.
struct log_sink_file_cfg_s {
        char path[LOG_SINK_FILE_CTX_MAX_PATH]; //file path
        unsigned max_files; //number of rotated files
        unsigned max_size; //max size per file in bytes
        unsigned flush_lines; //0: disable, N: fflush every N writes
        unsigned fsync_lines; //0: disable, N: fdatasync every N writes (after fflush) 
};
typedef struct log_sink_file_cfg_s log_sink_file_cfg_t;

int log_sink_file_reconfigure(log_sink_t *sink, const log_sink_file_cfg_t *cfg); //runtime reconfigure: supports flush_lines/fsync_lines only.
log_sink_t* log_sink_file_create(const log_sink_file_cfg_t *cfg);

#endif /*__LOG_SINK_FILE_H__*/
