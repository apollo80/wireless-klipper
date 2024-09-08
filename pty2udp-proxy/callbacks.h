/*
 * @file
 * @brief pty2udp proxy for klipper
 * @details
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#pragma once

#ifndef __pty2udp_proxy__callbacks_h__
#define __pty2udp_proxy__callbacks_h__

#include <event2/event.h>


/// @brief
/// @param[in] serial_fd
/// @param[in] events
/// @param[in] args
void cb_proxy__serial_recv(evutil_socket_t serial_fd, short events, void* args);

/// @brief
/// @param[in] serial_fd
/// @param[in] events
/// @param[in] args
void cb_proxy__network_recv(evutil_socket_t socket_fd, short events, void* args);

/// @brief
/// @param[in] serial_fd
/// @param[in] events
/// @param[in] args
void cb_proxy__resend_timeout(evutil_socket_t socket_fd, short events, void* args);

/// @brief
/// @param[in] serial_fd
/// @param[in] events
/// @param[in] args
void cb_proxy__sigTERM(int signal, short events, void* args);


#endif // __pty2udp_proxy__callbacks_h__
