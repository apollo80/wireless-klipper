/*
 * @file
 * @brief pty2udp proxy for klipper
 * @details
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "pty2udp-proxy.h"

#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>


/// @brief
/// @param[in] proxy
/// @return server address
static struct in_addr lookup_address(struct pty2udp_proxy* proxy);


int network_init(struct pty2udp_proxy* proxy) {
    if (NULL == proxy)
        return EXIT_FAILURE;

    //--- create socket
    proxy->network.socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (proxy->network.socket < 0) {
        LOG_CRITICAL(proxy, "failed socket(): %s", strerror(errno));
        return EXIT_FAILURE;
    }


    //--- resolve to address
    struct sockaddr_in* to_address = (struct sockaddr_in*) &(proxy->network.to_address);
    to_address->sin_family = AF_INET;
    to_address->sin_addr = lookup_address(proxy);
    to_address->sin_port = htons(proxy->network.to_port);

    if (0 == to_address->sin_addr.s_addr) {
        LOG_CRITICAL(proxy, "failed to resolved hostname '%s'", proxy->network.to_name);
        return EXIT_FAILURE;
    }


    //--- create socket event
    struct event* socket_event = event_new(proxy->ev_loop, proxy->network.socket, EV_READ|EV_PERSIST, cb__udp_recv, proxy);
    if (NULL == socket_event) {
        LOG_CRITICAL(proxy, "failed event_new(net) - %s", strerror(errno));

        close(proxy->network.socket);
        proxy->network.socket = -1;
        return EXIT_FAILURE;
    }
    proxy->network.ev_recv = socket_event;

    int ret = event_add(proxy->network.ev_recv, NULL);
    if (ret != 0) {
        LOG_CRITICAL(proxy, "failed event_add(net) - %s", strerror(errno));

        event_free(proxy->network.ev_recv);
        proxy->network.ev_recv = NULL;

        close(proxy->network.socket);
        proxy->network.socket = -1;
        return EXIT_FAILURE;
    }


    //--- create clientHello timeout event
    struct event* ev_timeout__clientHello = evtimer_new(proxy->ev_loop, cb_timeout__client_hello, proxy);
    if (NULL == ev_timeout__clientHello) {
        LOG_CRITICAL(proxy, "failed evtimer_new(clientHello) - %s", strerror(errno));

        // TODO: здесь нужно освобождать ресурсы
        return EXIT_FAILURE;
    }
    proxy->network.client_hello.ev_timeout      = ev_timeout__clientHello;
    proxy->network.client_hello.timeout.tv_sec  = 1;
    proxy->network.client_hello.timeout.tv_usec = 0;


    //--- create ping timeout event
    proxy->network.ping.timeout.tv_sec = 1;
    proxy->network.ping.timeout.tv_usec = 0;

    struct event* ev_timeout__send_ping = evtimer_new(proxy->ev_loop, cb_timeout__send_ping, proxy);
    if (NULL == ev_timeout__send_ping) {
        LOG_CRITICAL(proxy, "failed evtimer_new(send_ping) - %s", strerror(errno));

        // TODO: здесь нужно освобождать ресурсы
        return EXIT_FAILURE;
    }
    proxy->network.ping.send.ev = ev_timeout__send_ping;
    proxy->network.ping.send.timeout.tv_sec  = 0;
    proxy->network.ping.send.timeout.tv_usec = 200000;

    struct event* ev_timeout__recv_ping = evtimer_new(proxy->ev_loop, cb_timeout__recv_ping, proxy);
    if (NULL == ev_timeout__recv_ping) {
        LOG_CRITICAL(proxy, "failed evtimer_new(recv_ping) - %s", strerror(errno));

        // TODO: здесь нужно освобождать ресурсы
        return EXIT_FAILURE;
    }
    proxy->network.ping.recv.ev = ev_timeout__recv_ping;
    proxy->network.ping.recv.timeout.tv_sec  = 4;
    proxy->network.ping.recv.timeout.tv_usec = 0;
    proxy->network.ping.send.count = 0;
    proxy->network.ping.send.count_max = 6;


    //--- create resend event
    struct event* ev_timeout__resend = evtimer_new(proxy->ev_loop, cb_timeout__klipper_data, proxy);
    if (NULL == ev_timeout__resend) {
        LOG_CRITICAL(proxy, "failed evtimer_new(clientHello) - %s", strerror(errno));

        // TODO: здесь нужно освобождать ресурсы
        return EXIT_FAILURE;
    }
    proxy->resend.ev_timeout      = ev_timeout__resend;
    proxy->resend.timeout.tv_sec  = 0;
    proxy->resend.timeout.tv_usec = 100000;

    // send 'client hello'
    ret = net_send_client_hello(proxy);
    if (ret)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

int network_finish(struct pty2udp_proxy* proxy) {
    //--- free resend event
    event_free(proxy->resend.ev_timeout);
    proxy->resend.ev_timeout = NULL;


    //--- free
    // event_del(proxy->network.ping.recv.ev);
    event_free(proxy->network.ping.recv.ev);
    proxy->network.ping.recv.ev = NULL;

    // event_del(proxy->network.ping.send.ev);
    event_free(proxy->network.ping.send.ev);
    proxy->network.ping.send.ev = NULL;


    // free 'client hello'
    // event_del(proxy->network.client_hello.ev_timeout);
    event_free(proxy->network.client_hello.ev_timeout);
    proxy->network.client_hello.ev_timeout = NULL;


    // free receive socket
    event_free(proxy->network.ev_recv);
    proxy->network.ev_recv = NULL;

    // close socket
    close(proxy->network.socket);
    proxy->network.socket = -1;
}

struct in_addr lookup_address(struct pty2udp_proxy* proxy)
{
    struct in_addr ret_value;
    ret_value.s_addr = 0;

    if (NULL == proxy)
        return ret_value;

    // if (NULL == proxy->server_name)
    //     return ret_value;

    struct addrinfo hints;
    memset (&hints, 0, sizeof (hints));
    hints.ai_family   = PF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags   |= AI_CANONNAME;

    struct addrinfo *pai_parrent = NULL;
    struct addrinfo *pai_it = NULL;
    int err_code = getaddrinfo (proxy->network.to_name, NULL, &hints, &pai_parrent);
    if (err_code != 0) {
        LOG_CRITICAL(proxy, "failed getaddrinfo() - %s", gai_strerror(err_code));
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
