/*
 * @file
 * @brief esp32c2 uart2net bridge for klipper
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#pragma once
#ifndef __wireless_klipper__tasks_h__
#define __wireless_klipper__tasks_h__

#include "wk_settings.h"

void wifi_init();
void wifi_blink_start();

// TODO: void mdns_start();
// TODO: void mdns_stop();

void uart_init();
void uart2net_start();

void net2uart_start();
bool net2uart_is_started();


/// @brief  
void webctrl_start();

/// @brief 
void webctrl_stop();

#endif // __wireless_klipper__tasks_h__
