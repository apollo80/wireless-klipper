/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 * @detauls settings functions
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

// void mdns_start();
// void mdns_stop();

void tcp2uart_init();
void tcp2uart_start();
void tcp2uart_stop();


/// @brief  
void webctrl_start();

/// @brief 
void webctrl_stop();

#endif // __wireless_klipper__tasks_h__
