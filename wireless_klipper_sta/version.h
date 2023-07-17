/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 *
 * author: apollo80
 * @email: apollo80@list.ru
 */

#pragma once
#ifndef __wireless_klipper_sta__version_h__
#define __wireless_klipper_sta__version_h__

#include <stdint.h>

typedef struct
{
    uint8_t major;
    uint8_t minor;
    uint8_t revision;
    uint8_t bugfix;
} version_t;


/// @brief firmware_version
extern version_t firmware_version;

#endif // __wireless_klipper_sta__version_h__
