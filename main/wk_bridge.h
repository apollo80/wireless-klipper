/*
 * @file
 * @brief esp32c2 uart2net bridge for klipper
 *
 * author: apollo80
 * @email: apollo80@list.ru
 */

#pragma once
#ifndef __wireless_klipper__bridge_h__
#define __wireless_klipper__bridge_h__

#include "wk_settings.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>

#include <sys/cdefs.h>
#include <lwip/sockets.h>


/// @brief
struct bridge_config_t {
    struct settings_t* app_config;

    int                socket;
    struct sockaddr_in source_address;
    socklen_t          source_address_len;

    uint8_t            *net_rx_buffer;
    uint8_t            *uart_rx_buffer;

    uint32_t           msg_net_index;
    uint32_t           msg_uart_index;

    TimerHandle_t      timer__session;

    TaskHandle_t       task__net2uart;
    SemaphoreHandle_t  sem__net2uart;

    TaskHandle_t       task__uart2net;
    SemaphoreHandle_t  sem__uart2net;
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

#define prefix_netStart         (0x0A1B2C0D)
#define prefix_uartStart        (0x0A2B3C0D)

#define prefix_netData          (0x0A4B5C0D)
#define prefix_uartData         (0x0A6B7C0D)
#define prefix_udpLog           (0x0A7B7C0D)

#define prefix_netConfirm       (0x30405060)
#define prefix_uartConfirm      (0x40506070)


/// @brief 
void bridge_net2uart(void*);

/// @brief 
void bridge_uart2net(void*);

void bridge_uart2net__start(struct bridge_config_t *bridge_config);
void bridge_uart2net__stop(struct bridge_config_t *bridge_config);

void bridge_config_lock();
void bridge_config_unlock();

#define CONFIG_WK_UDP_LOG_ENABLE 0
#if CONFIG_WK_UDP_LOG_ENABLE
void udp_log(int socket, struct sockaddr_in* socket_address, const char* format, ...);
#else
#define udp_log(socket, socket_address, format, ...);
#endif

#endif // __wireless_klipper__bridge_h__
