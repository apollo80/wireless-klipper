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

// void mdns_start();
// void mdns_stop();

void uart_init();
void uart_reinit();

void tcp2uart_start();
void tcp2uart_stop();


/// @brief  
void webctrl_start();

/// @brief 
void webctrl_stop();

#endif // __wireless_klipper__tasks_h__
