
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/sendfile.h>

#include "util/log/internal/log_sink_file.h"

/// @brief Log sink file context structure.
struct file_ctx_s{
        pthread_mutex_t mt; ///< Mutex for thread safety.
        FILE *fp; ///< File descriptor
        char path[LOG_SINK_FILE_CTX_MAX_PATH]; //< File path.
        uint32_t max_files; //<Max. files
        size_t cur_sz; //Current file size.
        uint32_t line_cnt;// Write line count.
        uint32_t flush_lines; //0: disable, N: flush every N writes
        uint32_t fsync_lines; //0: disable, N: fdatasync every N writes (after fflush) 
        bool remain_first_done; ///< Remain first file done flag.
};
typedef struct file_ctx_s file_ctx_t;


/// @brief Get file size.
/// @param ctx Log file sink context.
/// @param out_size Output size pointer.
/// @return 0 on success, -1 on failure.
static int f_get_size(void *ctx, size_t *out_size) 
{
        file_ctx_t *c = (file_ctx_t*)ctx;
        if (!c || !out_size) {
                return -1;
        }
        pthread_mutex_lock(&c->mt);
        *out_size = c->cur_sz;
        pthread_mutex_unlock(&c->mt);
        return 0;
}

/// @brief Open log file sink.
/// @param ctx Log file sink context.
/// @return 0 on success, -1 on failure.
static int f_open(void *ctx) 
{
        file_ctx_t *c = (file_ctx_t*)ctx;
        if (!c) {
            return -1;
        }
        pthread_mutex_lock(&c->mt);
        c->fp = fopen(c->path, "a");
        if (!c->fp) {
                pthread_mutex_unlock(&c->mt);
                return -1;
        }
        struct stat st;
        if (stat(c->path, &st) == 0) {
                c->cur_sz = (size_t)st.st_size;
        }
        c->line_cnt = 0;
        pthread_mutex_unlock(&c->mt);
        return 0;
}

/// @brief Close log file sink.
/// @param ctx Log file sink context.
static void f_close(void *ctx) 
{
        file_ctx_t *c = (file_ctx_t*)ctx;
        if (!c) {
                return;
        }
        pthread_mutex_lock(&c->mt);
        if (c->fp) { 
                (void)fflush(c->fp);
                (void)fclose(c->fp);
                c->fp = NULL; 
        }
        pthread_mutex_unlock(&c->mt);
}

/// @brief Maybe flush and sync the log file.
/// @param c Log file sink context.
/// @return
static int f_maybe_flush_sync(file_ctx_t *c) 
{
        if (!c || !c->fp) {
                return 0;
        }

        if (c->flush_lines && (c->line_cnt % c->flush_lines) == 0) {
                if (fflush(c->fp) != 0) {
                        return -1;
                }
        }

        if (c->fsync_lines && (c->line_cnt % c->fsync_lines) == 0) {
                if (fflush(c->fp) != 0) {
                        return -1;
                }
                int fd = fileno(c->fp);
                if (fd >= 0) {
                        if (fdatasync(fd) != 0) {
                                return -1;
                        }
                }
        }
        return 0;
}

/// @brief Write log to file.
/// @param ctx Log file sink context.
/// @param buf Buffer containing log data.
/// @param len Length of the log data.
/// @return 0 on success, -1 on failure.
static int f_write(void *ctx, const char *buf, size_t len) 
{
        file_ctx_t *c = (file_ctx_t*)ctx;
        if (!c || !c->fp) {
                return -1;
        }
        pthread_mutex_lock(&c->mt);
        if (!c->fp) {
                pthread_mutex_unlock(&c->mt);
                return -1;
        }
        size_t w = fwrite(buf, 1, len, c->fp);
        if (w != len) {
                pthread_mutex_unlock(&c->mt);
                return -1;
        }
        c->cur_sz += w;
        c->line_cnt++;
        if (f_maybe_flush_sync(c) != 0) {
                pthread_mutex_unlock(&c->mt);
                return -1;
        }
        if ((c->line_cnt % 128) == 0) {
                struct stat st;
                if (stat(c->path, &st) == 0) {
                        c->cur_sz = (size_t)st.st_size;
                }
        }
        pthread_mutex_unlock(&c->mt);
        return 0;
}

/// @brief Flush log file.
/// @param ctx Log file sink context.
static void f_flush(void *ctx) 
{
        file_ctx_t *c = (file_ctx_t*)ctx;
        if (!c) {
                return;
        }
        pthread_mutex_lock(&c->mt);
        if (c->fp) {
                (void)fflush(c->fp);
        }
        pthread_mutex_unlock(&c->mt);
}

/// @brief Shift (rotate) log files.
/// @param c Log file sink context.
/// @param partial Output partial flag pointer.
/// @return rotate: 0 ok, 1 partial, -1 fail
static int do_shift(file_ctx_t *c, unsigned *partial) 
{
        char from[300]="", to[300]="";
        unsigned maxf = c->max_files ? c->max_files : 3;
        for (int i=(int)maxf-1; i>=1; --i) {
                snprintf(from, sizeof(from), "%s.%d", c->path, i);
                snprintf(to, sizeof(to), "%s.%d", c->path, i+1);
                if (rename(from, to) != 0) {
                        if (errno == ENOENT) {
                                continue;
                        }
                        *partial = 1;
                }
        }
        return 0;
}

/// @brief Copy file using sendfile.
/// @param src Source file path.
/// @param dst Destination file path.
/// @return 0 on success, -1 on failure.
static int copy_file_sendfile(const char *src, const char *dst)
{
        int in_fd, out_fd;
        struct stat st;

        in_fd = open(src, O_RDONLY);
        fstat(in_fd, &st);

        out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);

        if(sendfile(out_fd, in_fd, NULL, st.st_size) == -1) {
                close(in_fd);
                close(out_fd);
                return -1;
        }

        close(in_fd);
        close(out_fd);
        return 0;
}

/// @brief Rotate the log file.
/// @param ctx Log file sink context. 
/// @param remain_first Remain first log file.
/// @param rotated_path Rotated file path buffer.
/// @param rotated_sz Size of rotated file path buffer. 
/// @return 0 on success, 1 on partial success, -1 on failure.
static int f_rotate(void *ctx, bool remain_first, char *rotated_path, size_t rotated_sz) 
{
        file_ctx_t *c = (file_ctx_t*)ctx;
        if (!c) {
                return -1;
        }
        pthread_mutex_lock(&c->mt);
        unsigned partial = 0;

        if (c->fp) {
                (void)fflush(c->fp);
                (void)fclose(c->fp);
                c->fp = NULL;
        }
        do_shift(c, &partial);

        char dst1[300], tmp[360];
        snprintf(dst1, sizeof(dst1), "%s.1", c->path);
        snprintf(tmp, sizeof(tmp), "%s.tmp.%d.%ld", dst1, (int)getpid(), (long)time(NULL));
        if (rename(c->path, tmp) != 0) {
                FILE *in = fopen(c->path, "r");
                FILE *out = fopen(tmp, "w");
                if (!in || !out) {
                        if (in) {
                                fclose(in);
                        }
                        if (out) {
                                fclose(out);
                        }
                        c->fp = fopen(c->path, "a");
                        if (c->fp) {
                                struct stat st;
                                if (stat(c->path, &st) == 0) {
                                        c->cur_sz = (size_t)st.st_size;
                                }
                                pthread_mutex_unlock(&c->mt);
                        }
                        return -1;
                }
                char buf[4096];
                size_t n;
                while((n = fread(buf,1,sizeof(buf),in)) > 0) {
                        if (fwrite(buf,1,n,out)!=n) { 
                                partial = 1; 
                                break; 
                        }
                }
                fclose(in); 
                fclose(out);
                FILE *tr = fopen(c->path, "w");
                if (tr) {
                        fclose(tr); 
                } else {
                        partial = 1;
                }
        }
        if (rename(tmp, dst1) != 0) {
                rename(tmp, c->path);
                partial = 1;
        }

        if (rotated_path && rotated_sz) {
                snprintf(rotated_path, rotated_sz, "%s", dst1);
        }
        c->fp = fopen(c->path, "a");
        if (!c->fp) {
                rename(dst1, c->path);
                c->fp = fopen(c->path, "a");
                if (!c->fp) {
                        pthread_mutex_unlock(&c->mt);
                        return -1;
                }
                partial = 1;
        }
        struct stat st;
        if (stat(c->path, &st) == 0) {
                c->cur_sz = (size_t)st.st_size;
                c->line_cnt = 0;
        }
        if (remain_first) {
                char first[300];
                snprintf(first, sizeof(first), "%s.0", c->path);
                if (copy_file_sendfile(dst1, first) != 0) {
                        partial = 1;
                }
        }
        if (!partial) {
                c->remain_first_done = true;
        }
        pthread_mutex_unlock(&c->mt);
        return partial ? 1 : 0;
}

/// @brief Destroy log file sink.
/// @param s Log sink to be destroyed.
static void f_destroy(log_sink_t *s) 
{
        if (!s) {
                return;
        }
        file_ctx_t *c = (file_ctx_t*)s->ctx;
        if (c) { 
                f_close(c); 
                pthread_mutex_destroy(&c->mt);
                free(c); 
        }
        free(s);
}

static int f_get_first_remain_done(void *ctx, bool *out_remain_first_done)
{
        file_ctx_t *c = (file_ctx_t*)ctx;
        if (!c || !out_remain_first_done) {
                return -1;
        }
        pthread_mutex_lock(&c->mt);
        *out_remain_first_done = c->remain_first_done;
        pthread_mutex_unlock(&c->mt);
        return 0;
}

/// @brief Log sink file operations.
static const log_sink_ops_t g_ops = {
        .open   = f_open,
        .close  = f_close,
        .write  = f_write,
        .flush  = f_flush,
        .rotate = f_rotate,
        .get_size = f_get_size,
        .get_first_remain_done = f_get_first_remain_done,
        .get_stats = NULL,
        
};

/// @brief Reconfigure the log sink file.
/// @param s Log sink to be reconfigured.
/// @param cfg New configuration.
/// @return 0 on success, -1 on failure.
int log_sink_file_reconfigure(log_sink_t *s, const log_sink_file_cfg_t *cfg) 
{
        if (!s || !cfg) {
                return -1;
        }
        file_ctx_t *c = (file_ctx_t*)s->ctx;
        if (!c) {
                return -1;
        }
        pthread_mutex_lock(&c->mt);
        // path/max_files are static (require restart or sink re-create)
        if (cfg->path && cfg->path[0] && strcmp(cfg->path, c->path) != 0) {
                pthread_mutex_unlock(&c->mt);
                return -1;
        }
        c->flush_lines = cfg->flush_lines;
        c->fsync_lines = cfg->fsync_lines;
        pthread_mutex_unlock(&c->mt);
        return 0;
}

/// @brief Create a log sink file.
/// @param cfg Configuration for the log sink file.
/// @return Pointer to the created log sink, or NULL on failure.
log_sink_t* log_sink_file_create(const log_sink_file_cfg_t *cfg) 
{
        log_sink_t *s = (log_sink_t*)calloc(1, sizeof(*s));
        file_ctx_t *c = (file_ctx_t*)calloc(1, sizeof(*c));
        if (!s || !c) { 
                free(s); 
                free(c); 
                return NULL; 
        }
        pthread_mutex_init(&c->mt, NULL);

        snprintf(c->path, sizeof(c->path), "%s", (cfg && cfg->path) ? cfg->path : "/tmp/"LOG_SINK_FILE_NAME_PREFIX"");
        c->max_files = (cfg && cfg->max_files) ? cfg->max_files : LOG_SINK_FILE_CFG_MAX_FILE;
        c->flush_lines = (cfg) ? cfg->flush_lines : 0;
        c->fsync_lines = (cfg) ? cfg->fsync_lines : 0;
        s->ops = &g_ops;
        s->ctx = c;
        s->refcnt = 1;
        s->destroy = f_destroy;
        return s;
}
