/*
 * @file
 * @brief pty2udp proxy for klipper
 * @details
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#pragma once

#ifndef pty2udp_proxy__log_h
#define pty2udp_proxy__log_h

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>


// pre-define
struct pty2udp_proxy;

/// @brief log event level
typedef enum  {
    None = 0
    , TRACE
    , DEBUG
    , INFO
    , WARNING
    , ERROR
    , CRITICAL
} log_level_t;

/// @brief log settings
struct log_t {
    char        path[PATH_MAX];
    FILE*       file;
    size_t      file_size;
    log_level_t level;

    char        tmp_path[PATH_MAX];
    size_t      rotate_file_size;
    size_t      rotate_file_count;
};


/// @brief
/// @param[in] proxy
/// @return 0 in succeeded
int log_init(struct pty2udp_proxy* proxy);


/// @brief
void log_func(log_level_t level, struct pty2udp_proxy* proxy, const char* format, ... )  __attribute__ ((format (printf, 3, 4)));

/// @brief Convert binary array to hex string
/// @param[in] array binary array
/// @param[in] array_size size of binary array
/// @return array as hex string
char* array2hex(uint8_t* array, size_t array_size);


#define LOG_TRACE( proxy_cfg, ... )        log_func( TRACE,    proxy_cfg, __VA_ARGS__ )
#define LOG_DEBUG( proxy_cfg, ... )        log_func( DEBUG,    proxy_cfg, __VA_ARGS__ )
#define LOG_INFO( proxy_cfg, ... )         log_func( INFO,     proxy_cfg, __VA_ARGS__ )
#define LOG_WARNING( proxy_cfg, ... )      log_func( WARNING,  proxy_cfg, __VA_ARGS__ )
#define LOG_ERROR( proxy_cfg, ... )        log_func( ERROR,    proxy_cfg, __VA_ARGS__ )
#define LOG_CRITICAL( proxy_cfg, ... )     log_func( CRITICAL, proxy_cfg, __VA_ARGS__ )

#endif // pty2udp_proxy__log_h
