#pragma once

#include "internal_types.h"

fdb_status fdb_log_init(struct fdb_log_config log_config);
fdb_status fdb_log(err_log_callback *callback,
                   fdb_status status,
                   const char *format, ...);

struct fdb_log_config {
    fdb_log_config(): log_msg_level(1) {}

    size_t log_msg_level;
};

