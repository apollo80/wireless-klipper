/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 *
 * author: apollo80
 * @email: apollo80@list.ru
 */

#pragma once
#ifndef __wireless_klipper_sta__settings_h__
#define __wireless_klipper_sta__settings_h__

#include "version.h"


/* * * * * * * 
 * EEPROM settings
 */
struct settings_t
{
    /// @brief firmware version
    version_t version;


    /// @brief wifi point name
    char wifi_hostname[36];

    /// @brief wifi SSID
    char wifi_ssid[36];

    /// @brief wifi password
    char wifi_password[36];

    /// @brief wifi mode
    bool wifi_use_sta;


    /// @brief speed of serial port
    uint32_t serialPort_baud;

    /// @brief serial port buffer size for receiving data
    uint16_t serialPort_rxBuffSize;


    /// @brief port of tcp2serial server
    uint16_t tcpServer_port;

    // buffer size for receiving/transmitting data
    uint16_t tcpServer_buffSize;


    /// @brief Sign of using static network addressing
    bool use_static_ip;

    /// @brief static network addressing module
    uint8_t static_IPaddress[4];

    /// @brief network module when using static network addressing
    uint8_t static_netmask[4];

    /// @brief gateway address when using static addressing
    uint8_t static_gateway[4];
};


/// @brief default configuration
extern struct settings_t moduleSettings;

#endif // __wireless_klipper_sta__settings_h__
