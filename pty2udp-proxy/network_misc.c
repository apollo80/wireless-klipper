/*
 * @file
 * @brief pty2udp proxy for klipper
 * @details
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "pty2udp-proxy.h"
#include "log.h"

#include <assert.h>
#include <arpa/inet.h>


void net_send_klipper_data(struct pty2udp_proxy* proxy) {

    ssize_t nwrite = sendto(proxy->network.socket
            , proxy->serial.send_pkg, proxy->serial.send_pkg_size
            , MSG_DONTWAIT
            , &(proxy->network.to_address), sizeof(proxy->network.to_address));

    assert(nwrite == proxy->serial.send_pkg_size);

#ifndef NDEBUG
    char address_as_str[128];
    struct sockaddr_in* tmp = (struct sockaddr_in*) &(proxy->network.to_address);
    struct msg_header_t *header = (struct msg_header_t *) proxy->serial.send_pkg;

    inet_ntop(AF_INET, &(tmp->sin_addr), address_as_str, sizeof(address_as_str));
    LOG_DEBUG(proxy, "pty -> net: send %zi (= %zi + %zi) bytes to %s:%u, net index %u, prefix 0x%0x;"
        , nwrite
        , sizeof(struct msg_header_t)
        , (proxy->serial.send_pkg_size - sizeof(struct msg_header_t))
        , address_as_str, ntohs(tmp->sin_port)
        , header->msg_index, header->msg_prefix);
#endif
}

