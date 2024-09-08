/*
 * @file
 * @brief pty2udp proxy for klipper
 * @details
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "log.h"
#include "pty2udp-proxy.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static const char* log_level_to_string[] = {
        "none"
        , "trace"
        , "debug"
        , "info"
        , "warning"
        , "error"
        , "critical"
};

void log_func(log_level_t level, struct pty2udp_proxy* proxy, const char* format, ...) {
    static char log_buffer[1024];
    // char time_buff[128];

    if (level > CRITICAL)
        level = None;

    if (proxy && proxy->log_level > level)
        return;

    FILE* log_out = NULL;
    if (proxy) {
        if (proxy->log_file)
            log_out = proxy->log_file;
    } else
        log_out = stderr;

    if (!log_out)
        return;

    struct timespec ts;
    timespec_get(&ts, TIME_UTC);

    size_t prefix_offset = 0;

    // if (proxy_cfg->log_impl == "stdout") {
    prefix_offset += strftime(log_buffer, sizeof(log_buffer), "%Y.%m.%d %T", gmtime(&ts.tv_sec));
    prefix_offset += snprintf(log_buffer + prefix_offset, sizeof(log_buffer) - prefix_offset
            ,".%06lu - %8s - ", ts.tv_nsec / 1000, log_level_to_string[level]);
    // }

    va_list args;
    va_start(args, format);
    prefix_offset += vsnprintf(log_buffer + prefix_offset, sizeof(log_buffer) - prefix_offset, format, args);
    va_end(args);

    // if (proxy_cfg->log_impl == "stdout") {
    log_buffer[prefix_offset] = '\n';
    log_buffer[prefix_offset + 1] = 0;
    fputs(log_buffer, log_out);
    // fflush(proxy->log_file);
    // }
}

char* array2hex(uint8_t* array, size_t array_size) {
    static char hex_buff[4096];

    for(size_t idx = 0; idx < array_size; ++idx) {
        snprintf(hex_buff + idx*3, sizeof(hex_buff) - idx*3, "%02x ", array[idx]);
    }
    hex_buff[array_size*3 - 1] = 0;

    return hex_buff;
}