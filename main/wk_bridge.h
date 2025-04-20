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
#include <lwip/udp.h>


/// @brief
struct bridge_config_t {
    struct settings_t* app_config;

    struct udp_pcb*   udp_socket;
    ip_addr_t         source_address;
    uint16_t          source_port;

    uint8_t            *uart_rx_buffer;
    uint8_t            *send_buffer;

    uint8_t           msg_net_index;
    uint8_t           msg_uart_index;

    TimerHandle_t      timer__session;
    TaskHandle_t       task__uart2net;

    SemaphoreHandle_t  sem__net2uart;
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

#define prefix_clientHello          (0x01)
#define prefix_serverHello          (0x02)

#define prefix_klipperData          (0x03)
#define prefix_klipperDataConfirm   (0x04)

#define prefix_mcuData              (0x05)
#define prefix_mcuDataConfirm       (0x06)

#define prefix_clnPingReq           (0x0E)
#define prefix_srvPingRep           (0x0F)

#define prefix_udpLog               (0x81)


/// @brief
void task_uart2net(void*);

void bridge_config_lock();
void bridge_config_unlock();

bool send_uart_data(struct bridge_config_t *bridge_config, size_t data_size);
void write_to_uart(struct bridge_config_t *bridge_config, const char* buffer, size_t buf_size);

#define CONFIG_WK_UDP_LOG_ENABLE 0
#if CONFIG_WK_UDP_LOG_ENABLE
void udp_log(int socket, struct sockaddr_in* socket_address, const char* format, ...);
#else
#define udp_log(socket, socket_address, format, ...);
#endif

#endif // __wireless_klipper__bridge_h__
