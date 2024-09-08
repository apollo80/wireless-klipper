/*
 * @file
 * @brief pty2udp proxy for klipper
 * @details
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#pragma once

#ifndef __pty2udp_proxy__config_h__
#define __pty2udp_proxy__config_h__

#include <stdlib.h>


/// @brief
/// @param[in] cfg_filename
void config_read(const char* cfg_filename);

#endif // __pty2udp_proxy__config_h__
