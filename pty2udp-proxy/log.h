/*
 * @file
 * @brief pty2udp proxy for klipper
 * @details
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#pragma once

#ifndef __pty2udp_proxy__log_h__
#define __pty2udp_proxy__log_h__

#include <stddef.h>
#include <stdint.h>


typedef enum  {
    None = 0
    , TRACE
    , DEBUG
    , INFO
    , WARNING
    , ERROR
    , CRITICAL
} log_level_t;

struct pty2udp_proxy;

void log_func(log_level_t level, struct pty2udp_proxy* proxy_cfg,  const char* format, ... )  __attribute__ ((format (printf, 3, 4)));

char* array2hex(uint8_t* array, size_t array_size);

#define LOG_TRACE( proxy_cfg, ... )        log_func( TRACE, proxy_cfg, __VA_ARGS__ )
#define LOG_DEBUG( proxy_cfg, ... )        log_func( DEBUG, proxy_cfg, __VA_ARGS__ )
#define LOG_INFO( proxy_cfg, ... )         log_func( INFO, proxy_cfg, __VA_ARGS__ )
#define LOG_WARNING( proxy_cfg, ... )      log_func( WARNING, proxy_cfg, __VA_ARGS__ )
#define LOG_ERROR( proxy_cfg, ... )        log_func( ERROR, proxy_cfg, __VA_ARGS__ )
#define LOG_CRITICAL( proxy_cfg, ... )     log_func( CRITICAL, proxy_cfg, __VA_ARGS__ )

#endif // __pty2udp_proxy__log_h__
