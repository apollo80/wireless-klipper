/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 *
 * author: apollo80
 * @email: apollo80@list.ru
 */

#pragma once
#ifndef __wireless_klipper__settings_h__
#define __wireless_klipper__settings_h__

#include "wk_version.h"

#include <sdkconfig.h>
#include "freertos/FreeRTOS.h"
#include "driver/uart.h"
#include "esp_wifi_types.h"


/* * * * * * * 
 * Module settings
 */
struct settings_t
{
    /// @brief firmware version
    union {
        version_t ver;
        uint32_t  raw;
    } version;

    /// @brief wifi point name
    char wifi_hostname[32];

    /// @brief wifi SSID
    char wifi_ssid[32];

    /// @brief wifi password
    char wifi_password[64];

    /// @brief wifi mode
    bool wifi_use_sta;


    /// @brief speed of serial port
    uint32_t uart_baud_rate;

    /// @brief serial port buffer size for receiving data
    size_t uart_rx_buffer_size;


    /// @brief port of tcp2serial server
    uint16_t net_port;

    // buffer size for receiving/transmitting data
    size_t net_rx_buffer_size;

/*
    /// @brief Sign of using static network addressing
    bool use_static_ip;

    /// @brief static network addressing module
    uint8_t static_IPaddress[4];

    /// @brief network module when using static network addressing
    uint8_t static_netmask[4];

    /// @brief gateway address when using static addressing
    uint8_t static_gateway[4];
*/
};

/// @brief 
/// @return
struct settings_t* app_config();

/// @brief 
void app_config_read();

/// @brief 
void app_config_write();

#endif // __wireless_klipper_sta__settings_h__
