#pragma once

#include "internal_types.h"

typedef size_t fdb_log_levels;
enum {
    FDB_LOG_FATAL = 1,
    FDB_LOG_ERROR = 2,
    FDB_LOG_WARNING = 3,
    FDB_LOG_INFO = 4,
    FDB_LOG_DEBUG = 5,
    FDB_LOG_TRACE = 6
};

fdb_status fdb_log_init(struct fdb_log_config log_config);

#define fdb_log(cb, lv, s, ...) \
    fdb_log_impl(cb, lv, s, __FILE__, __func__, __LINE__, __VA_ARGS__)

fdb_status fdb_log_set_global_callback(err_log_callback* log_callback);

fdb_status fdb_log_impl(err_log_callback* log_callback,
                        fdb_log_levels given_log_level,
                        fdb_status status,
                        const char* source_file,
                        const char* func_name,
                        size_t line_number,
                        const char *format, ...);

struct fdb_log_config {
    fdb_log_config(): log_msg_level(1) {}

    size_t log_msg_level;
};

