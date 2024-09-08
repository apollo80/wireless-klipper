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

#include <string.h>
#include <unistd.h>

/// @brief
/// @param[in] hostname
/// @return server address
static struct in_addr lookup_address(struct pty2udp_proxy* proxy);

struct event* pty2udp_proxy_init_network(struct pty2udp_proxy* proxy) {

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        perror("failed socket(): ");
        return NULL;
    }
    proxy->socket_fd = socket_fd;
    proxy->session_start = 0;

    struct sockaddr_in* sock_address = (struct sockaddr_in*) &(proxy->server_address);
    sock_address->sin_family = AF_INET;
    sock_address->sin_addr = lookup_address(proxy);
    sock_address->sin_port = htons(proxy->server_port);

    if (0 == sock_address->sin_addr.s_addr) {
        fprintf(stderr, "failed to resolved hostname '%s'", proxy->server_name);
        return NULL;
    }

    struct event* socket_event = event_new(proxy->event_base, proxy->socket_fd, EV_READ|EV_PERSIST, cb_proxy__network_recv, proxy);
    if (NULL == socket_event) {
        LOG_CRITICAL(proxy, "failed event_new(net) - %s", strerror(errno));

        close(proxy->socket_fd);
        proxy->socket_fd = -1;
        return NULL;
    }
    proxy->network_event = socket_event;

    proxy->resend_timeout_event = event_new(proxy->event_base, -1, 0, cb_proxy__resend_timeout, proxy);
    if (NULL == proxy->resend_timeout_event) {
        LOG_CRITICAL(proxy, "failed event_new(net_timeout) - %s", strerror(errno));

        event_free(proxy->network_event);
        proxy->network_event = NULL;

        close(proxy->socket_fd);
        proxy->socket_fd = -1;

        return NULL;
    }

    proxy->resend_timeout.tv_sec = 0;
    proxy->resend_timeout.tv_usec = 115 * 100;
    proxy->resend_count = 0;
    proxy->resend_count_max = 50;

    proxy->serial_data_size   = 0;
    proxy->msg_net_confirmed  = true;
    proxy->first_uart_message = true;

    return socket_event;
}

struct in_addr lookup_address(struct pty2udp_proxy* proxy)
{
    struct in_addr ret_value;
    ret_value.s_addr = 0;

    if (NULL == proxy)
        return ret_value;

    if (NULL == proxy->server_name)
        return ret_value;


    struct addrinfo hints;
    memset (&hints, 0, sizeof (hints));
    hints.ai_family   = PF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags   |= AI_CANONNAME;

    struct addrinfo *pai_parrent = NULL;
    struct addrinfo *pai_it = NULL;
    int errcode = getaddrinfo (proxy->server_name, NULL, &hints, &pai_parrent);
    if (errcode != 0) {
        LOG_CRITICAL(proxy, "failed getaddrinfo() - %s", gai_strerror(errcode));
        freeaddrinfo(pai_parrent);
        return ret_value;
    }

    pai_it = pai_parrent;
    // while (result)
    if (pai_it)
    {
        struct sockaddr_in* resolved_address = (struct sockaddr_in *) pai_it->ai_addr;
        ret_value = resolved_address->sin_addr;

#ifndef NDEBUG
        char address_as_str[128];
        inet_ntop(pai_it->ai_family, &(ret_value), address_as_str, sizeof(address_as_str));
        LOG_DEBUG(proxy, "resolve hostname to IPv4 address: %s (%s)", address_as_str, pai_it->ai_canonname);

        // pai = pai->ai_next;
#endif
    }

    freeaddrinfo(pai_parrent);
    return ret_value;
}
