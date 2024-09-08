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
#include <unistd.h>
#include <string.h>


void cb_proxy__resend_timeout(evutil_socket_t socket_fd, short events, void* args) {
    struct pty2udp_proxy* proxy_settings = args;
    if (proxy_settings->serial_data_size == 0) {
        return;
    }

    ssize_t nwrite = sendto(proxy_settings->socket_fd
            , proxy_settings->serial_rx, proxy_settings->serial_data_size + sizeof(struct msg_header_t)
            , 0
            , &(proxy_settings->server_address), sizeof(proxy_settings->server_address));

    assert(nwrite == (proxy_settings->serial_data_size + sizeof(struct msg_header_t)));

    proxy_settings->resend_count++;

#ifndef NDEBUG
#ifndef NDEBUG
    char address_as_str[128];
    struct sockaddr_in* tmp = (struct sockaddr_in*) &(proxy_settings->server_address);

    struct msg_header_t* header = (struct msg_header_t*) proxy_settings->serial_rx;

    inet_ntop(AF_INET, &(tmp->sin_addr), address_as_str, sizeof(address_as_str));
    LOG_DEBUG(proxy_settings, "pty -> net: re send %zi (= %zi + %zi) bytes to %s:%u, net index %u, prefix 0x%0x"
        , nwrite, proxy_settings->serial_data_size, sizeof(struct msg_header_t)
        , address_as_str, ntohs(tmp->sin_port)
        , ntohl(header->net_index), ntohl(header->msg_prefix));
#endif
#endif

    if (proxy_settings->resend_count >= proxy_settings->resend_count_max) {
        LOG_DEBUG(proxy_settings, "cb net timeout: !! session reset !!");
        proxy_settings->resend_count = 0;
        return;
    }

    event_add(proxy_settings->resend_timeout_event, &(proxy_settings->resend_timeout));
}

