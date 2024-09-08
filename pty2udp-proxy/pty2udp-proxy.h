/*
 * @file
 * @brief pty2udp proxy for klipper
 * @details
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#pragma once

#ifndef __pty2udp_proxy_h__
#define __pty2udp_proxy_h__

#include "log.h"

#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <arpa/inet.h>

#include <event2/event.h>


/// @brief
struct pty2udp_proxy {
    struct event_base* event_base;

    // -- serial
    char          serial_path[PATH_MAX];
    uint32_t      serial_baud;
    int           serial_fd;
    int           serial_fd_slave;
    uint8_t       serial_rx[2048];
    size_t        serial_data_size;
    struct event* serial_event;

    // -- network
    char            server_name[PATH_MAX];
    in_port_t       server_port;
    int             socket_fd;
    uint8_t         socket_rx[2048];
    struct event*   network_event;
    struct sockaddr server_address;

    // -- session
    int session_start;

    uint32_t msg_net_index;
    uint32_t msg_uart_index;

    bool first_uart_message;
    bool msg_net_confirmed;

    struct event* resend_timeout_event;
    struct timeval resend_timeout;
    size_t resend_count;
    size_t resend_count_max;

    // --
    char        log_path[PATH_MAX];
    FILE*       log_file;
    log_level_t log_level;
};

/// @brief
struct msg_header_t {
    uint32_t msg_prefix;
    uint32_t net_index;
    uint32_t uart_index;
    uint32_t msg_size;
};

/// @brief
union uni_header_t {
    struct msg_header_t header;
    uint8_t raw[sizeof(struct msg_header_t)];
};

// #define header_size         16

#define prefix_netStart     (0x0A1B2C0D)
#define prefix_uartStart    (0x0A2B3C0D)

#define prefix_netData      (0x0A4B5C0D)
#define prefix_uartData     (0x0A6B7C0D)
#define prefix_udpLog       (0x0A7B7C0D)

#define prefix_netConfirm   (0x30405060)
#define prefix_uartConfirm  (0x40506070)


/// @brief
/// @param[in] idx_setting
struct pty2udp_proxy*  pty2udp_proxy_new();


/// @brief
/// @return
size_t pty2udp_proxy_count();

/// @brief
/// @param[in] proxy_index
/// @return
struct pty2udp_proxy* pty2udp_proxy_get(size_t proxy_index);

/// @brief
/// @param[in] idx_setting
/// @return
struct pty2udp_proxy* pty2udp_proxy_init(size_t proxy_index, struct event_base *base);

/// @brief
/// @param[in] idx_setting
void pty2udp_proxy_reset(size_t idx_setting);


/// @brief
/// @param[in] base
/// @param[in] idx_settings
/// @param[in] serial_path
/// @param[in] serial_baud
struct event* init_serial(struct event_base *base, size_t idx_settings);

/// @brief
/// @param[in] proxy_settings
// void reinit_serial(struct pty2udp_proxy* proxy_settings);

/// @brief
/// @param[in] base
/// @param[in] idx_settings
/// @param[in] server_name
/// @param[in] server_port
// struct event* init_network(struct event_base *base, size_t idx_settings);


#endif // __pty2udp_proxy_h__

