#include <stdarg.h>
#if !defined(WIN32) && !defined(_WIN32)
#include <sys/time.h>
#endif
#include <time.h>

#include "log_message.h"


void printISOTime(char* buffer, size_t buffer_len) {
    struct tm* tm_now;
    time_t rawtime;
    time(&rawtime);
    tm_now = localtime(&rawtime);

    // 2017-06-22T10:00:00
    size_t time_len = strftime(buffer, buffer_len,
                               "%Y-%m-%dT%H:%M:%S", tm_now);

    // Add milliseconds
    timeval cur_time;
    gettimeofday(&cur_time, NULL);
    size_t milli = cur_time.tv_usec / 1000;
    // 2017-06-22T10:00:00.123
    sprintf(buffer + time_len, ".%03d", (int)milli);
    time_len += 4;

    // timezone offset format: -0500
    char tz_offset_str[6];
    size_t offset_len =  strftime(tz_offset_str, 6,
                                  "%z", tm_now);
    if (offset_len < 5) {
        // Time zone info is not supported, skip it.
        return;
    }

    // hour
    strncat(buffer, tz_offset_str, 3);
    // :
    strcat(buffer, ":");
    // min
    strncat(buffer, tz_offset_str + 3, 2);
    // final format: 2017-06-22T10:00:00.123-05:00
}

// Log config that is globally used for this process.
static fdb_log_config global_log_config;

fdb_status fdb_log_init(struct fdb_log_config log_config) {
    if (log_config.log_msg_level > 6) {
        return FDB_RESULT_INVALID_CONFIG;
    }
    global_log_config = log_config;
    return FDB_RESULT_SUCCESS;
}

fdb_status fdb_log(err_log_callback *log_callback,
                   fdb_log_levels given_log_level,
                   fdb_status status,
                   const char *format, ...)
{
    // 1: Fatal   [FATL]
    // 2: Error   [ERRO]
    // 3: Warning [WARN]
    // 4: Info    [INFO]
    // 5: Debug   [DEBG]
    // 6: Trace   [TRAC]
    size_t cur_log_level = given_log_level;

    if (global_log_config.log_msg_level < cur_log_level) {
        // Configuration doesn't allow to print out
        // log message of this level.
        return status;
    }

    char msg[4096];
    va_list args;
    va_start(args, format);
    vsprintf(msg, format, args);
    va_end(args);

    if (log_callback && log_callback->callback) {
        log_callback->callback(status, msg, log_callback->ctx_data);
    } else {
        char ISO_time_buffer[64];
        char log_abbr[7][8] = {"XXXX", "FATL", "ERRO", "WARN",
                               "INFO", "DEBG", "TRAC"};
        printISOTime(ISO_time_buffer, 64);
        if (status != FDB_RESULT_SUCCESS) {
            fprintf(stderr, "%s [%s][FDB] %s\n",
                    ISO_time_buffer, log_abbr[cur_log_level], msg);
        } else {
            fprintf(stderr, "%s [%s][FDB] %s\n",
                    ISO_time_buffer, log_abbr[cur_log_level], msg);
        }
    }
    return status;
}

