/*
 * @file
 * @brief pty2udp proxy for klipper
 * @details
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#pragma once

#ifndef pty2udp_proxy_h
#define pty2udp_proxy_h

#include "log.h"

#include <stdlib.h>
#include <limits.h>

#include <event2/event.h>
#include <netinet/in.h>


struct p2u_serial_t { ;
    char            path[PATH_MAX];
    uint32_t        baud;

    int             fd_master;
    int             fd_slave;

    uint8_t         rx_buffer[2048];
    size_t          rx_data_size;

    uint8_t         send_pkg[2048];
    size_t          send_pkg_size;

    struct event*   ev;
};

struct p2u_network_t { ;
    char            to_name[PATH_MAX];
    in_port_t       to_port;

    struct sockaddr to_address;
    int             socket;

    struct event*   ev_recv;

    struct { ;
        struct event   *ev_timeout;
        struct timeval timeout;
    } client_hello;

    struct {
        struct {
            struct event *ev;
            struct timeval timeout;

            int count;
            int count_max;
        } send;

        struct {
            struct event *ev;
            struct timeval timeout;
        } recv;

        struct timeval timeout;
    } ping;

    uint8_t         rx_buffer[2048];
    size_t          rx_pkg_size;
};


/// @brief
struct pty2udp_proxy {
    struct event_base* ev_loop;

    // -- serial
    struct p2u_serial_t  serial;
    struct p2u_network_t network;

    // -- session
    uint32_t msg_index_klipper;
    uint32_t msg_index_mcu;

    struct {
        struct event*  ev_timeout;
        struct timeval timeout;

        size_t count;
    } resend;

    // --
    struct log_t log;
};

/// @brief
struct msg_header_t {
    uint8_t  msg_prefix;
    uint8_t  msg_index;
    uint16_t msg_size;
};

/// @brief
union uni_header_t {
    struct msg_header_t header;
    uint8_t raw[sizeof(struct msg_header_t)];
};

// #define header_size         16

// message format:
// |            header             | ...
// | 1 bytes | 1 bytes   | 2 bytes |        data       |
// | prefix  | index = 0 |       4 | server IP address |
#define prefix_clientHello          (0x01)

// | 1 bytes | 1 bytes   | 2 bytes |        data       |
// | prefix  | index = 0 |       4 | server IP address |
#define prefix_serverHello          (0x02)

// | 1 bytes | 1 bytes |  2 bytes  |     data     |
// | prefix  | index   | data size | klipper data |
#define prefix_klipperData          (0x03)

// | 1 bytes | 1 bytes |  2 bytes  |     data     |
// | prefix  |  index  |    ???    |
#define prefix_klipperDataConfirm   (0x04)

// | 1 bytes | 1 bytes |  2 bytes  |     data     |
// | prefix  | index   | data size |   mcu data   |
#define prefix_mcuData              (0x05)

// | 1 bytes | 1 bytes |  2 bytes  |     data     |
// | prefix  |  index  |    ???    |
#define prefix_mcuDataConfirm       (0x06)

#define prefix_clnPingReq           (0x0E)
#define prefix_srvPingRep           (0x0F)

#define prefix_udpLog               (0x81)


/// @brief
/// @param[in] idx_setting
struct pty2udp_proxy*  pty2udp_proxy_new();

/// @brief
/// @return
size_t pty2udp_proxy_count();

/// @brief
/// @param[in] idx_setting
/// @return
int pty2udp_proxy_init(size_t proxy_index, struct event_base *ev_loop);

/// @brief
/// @param[in] idx_setting
void pty2udp_proxy_reset(size_t idx_setting);

/***
 * Serial functions
 */

void cb__serial_recv(evutil_socket_t socket, short events, void* arg);

/// @brief
/// @param[in] proxy
/// @return
int serial_init(struct pty2udp_proxy* proxy);

/// @brief
/// @param[in] proxy
/// @return
int serial_finish(struct pty2udp_proxy* proxy);

/***
 * Network functions
 */

void cb__udp_recv(evutil_socket_t socket, short events, void* arg);


/// @brief
/// @param[in] proxy
/// @return
int network_init(struct pty2udp_proxy* proxy);

/// @brief
/// @param[in] proxy
/// @return
int network_finish(struct pty2udp_proxy* proxy);

/// @brief
/// @param[in] proxy
/// @return
int net_send_client_hello(struct pty2udp_proxy* proxy);

/// @brief
/// @param[in] proxy
/// @return
void net_recv_server_hello(struct pty2udp_proxy* proxy);

/// @brief
/// @param[in] proxy
/// @return
int net_send_ping(struct pty2udp_proxy* proxy);

/// @brief
/// @param[in] proxy
/// @return
void net_recv_ping(struct pty2udp_proxy* proxy);

/// @brief
/// @param[in] proxy
/// @return
void net_ping_update_timer(struct pty2udp_proxy* proxy);


/// @brief
/// @param[in] proxy
/// @return
void net_send_klipper_data(struct pty2udp_proxy* proxy);

/// @brief
/// @param[in] proxy
/// @return
void net_recv_klipper_data_confirm(struct pty2udp_proxy* proxy);

/// @brief
/// @param[in] proxy
/// @return
void net_recv_mcu_data(struct pty2udp_proxy* proxy);

/***
 * Timeout functions
 * */

/// @brief
/// @param[in] socket
/// @param[in] events
/// @param[in] arg
/// @return
void cb_timeout__client_hello(evutil_socket_t socket, short events, void* arg);

/// @brief
/// @param[in] socket
/// @param[in] events
/// @param[in] arg
/// @return
void cb_timeout__send_ping(evutil_socket_t socket, short events, void* arg);

/// @brief
/// @param[in] socket
/// @param[in] events
/// @param[in] arg
/// @return
void cb_timeout__recv_ping(evutil_socket_t socket, short events, void* arg);

/// @brief
/// @param[in] socket
/// @param[in] events
/// @param[in] arg
/// @return
void cb_timeout__klipper_data(evutil_socket_t serial_fd, short events, void* args);

#endif // pty2udp_proxy_h
