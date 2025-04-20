/*
 * @file
 * @brief udp_log for klipper
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#pragma once
#ifndef __wireless_klipper__udp_log_h__
#define __wireless_klipper__udp_log_h__

#include <lwip/udp.h>


/// @brief
/// @param[in] ipaddr
/// @param[in] port
/// @return
int udp_log_init(const ip_addr_t* to_address, uint16_t port);

/// @brief
void udp_log_free(void);

#endif // __wireless_klipper__udp_log_h__
