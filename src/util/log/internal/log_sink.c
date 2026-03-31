#include <stdlib.h>
#include "util/log/internal/log_sink.h"

/// @brief Increment the reference count of the log sink.
/// @param sink Pointer to the log sink.
/// @return Pointer to the log sink.
log_sink_t* log_sink_ref(log_sink_t *sink)
{
        if (!sink) {
                return NULL;
        }
        __sync_add_and_fetch(&sink->refcnt, 1);
        return sink;
}

/// @brief Decrement the reference count of the log sink and destroy it if the count reaches zero.
/// @param sink Pointer to the log sink.
void log_sink_unref(log_sink_t *sink) {
        if (!sink) {
                return;
        }
        if (__sync_sub_and_fetch(&sink->refcnt, 1) == 0) {
                void (*destroy_fn)(log_sink_t*) = sink->destroy;
                sink->destroy = NULL;
                if (destroy_fn) {
                        destroy_fn(sink);
                }
                else free(sink);
        }
}
