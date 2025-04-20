/*
 * @file
 * @brief pty2udp proxy for klipper
 * @details
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "callbacks.h"
#include "log.h"
#include "pty2udp-proxy.h"

#include <assert.h>
#include <string.h>



void cb_timeout__klipper_data(evutil_socket_t serial_fd, short events, void* args) {
    assert(args != NULL);

    struct pty2udp_proxy *proxy = args;
    // assert(serial_fd == proxy->serial.fd_master);
    LOG_WARNING(proxy, "pty -> net: timeout - confirm from mcu not received; try send klipper data again");

    net_send_klipper_data(proxy);

    // wait confirm message
    event_add(proxy->resend.ev_timeout, &(proxy->resend.timeout));
}
