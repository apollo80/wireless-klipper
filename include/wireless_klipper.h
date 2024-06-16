/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 *
 * author: apollo80
 * @email: apollo80@list.ru
 */

#pragma once
#ifndef __wireless_klipper_h__
#define __wireless_klipper_h__

#include "settings.h"
#include <Esp.h>

#if defined(DEBUG_ESP_PORT)
#define ENABLE_DEBUG 1
#endif

/* * * * * * *
 * WiFi functions
 */
/// @brief initializes WiFi and starts the process of connecting to an access point
void init_wifi();

/// @brief
void wifi_update(bool existClient);


/* * * * * * *
 * tcp2serial proxy functions
 */
/// @brief initializes the tcp2serial server
void init_tcp2serial();

/// @brief handles requests to the tcp2serial 
/// @return true if clients exist, false otherwise
bool handle_tcp2serial();


/* * * * * * *
 * http server functions
 */
 /// @brief initializes the tcp2serial server
void init_httpServer();

/// @brief handles requests to the http server
void handle_httpServer();


/* * * * * * * 
 * EEPROM functions
 */

/// @brief read setting from EEPROM
bool read_settings();

/// @brief write setting to EEPROM
void write_settings(settings_t& cfg);


/* * * * * * * 
 * debug functions
 */

#if ENABLE_DEBUG
// void printClientStatus();
// void printClientIP();
void printModuleSettings(settings_t& cfg);
#endif


#endif // __wireless_klipper_sta_h__
