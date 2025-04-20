/*
 * @file
 * @brief pty2udp proxy for klipper
 * @details
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "log.h"
#include "pty2udp-proxy.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <event2/event.h>

static const size_t ping_data_size = sizeof(struct msg_header_t) + sizeof(struct in_addr);

int net_send_ping(struct pty2udp_proxy* proxy) {
    if (NULL == proxy)
        return EXIT_FAILURE;

    uint8_t tx_buffer[ping_data_size];
    struct msg_header_t* header = (struct msg_header_t*) tx_buffer;
    uint8_t* tx_data = tx_buffer + sizeof(struct msg_header_t);

    header->msg_prefix = prefix_clnPingReq;
    header->msg_index  = 0;
    header->msg_size   = htons(sizeof(struct in_addr));

    struct sockaddr_in* sock_address = (struct sockaddr_in*) &(proxy->network.to_address);
    memcpy(tx_data, &(sock_address->sin_addr), sizeof(struct in_addr));

    ssize_t ret = sendto(proxy->network.socket, tx_buffer, ping_data_size
            , 0 , &(proxy->network.to_address), sizeof(proxy->network.to_address));
    if (ret == -1) {
        LOG_CRITICAL(proxy, "net_send_ping(): failed sendto() - %s", strerror(errno));
        return EXIT_FAILURE;
    }

    // resending, in case the first ping packet is lost
    ret = evtimer_add(proxy->network.ping.send.ev, &(proxy->network.ping.send.timeout));
    // TODO: здесь нужно обрабатывать 'ret'

    return EXIT_SUCCESS;
}

void net_recv_ping(struct pty2udp_proxy* proxy) {
    assert(proxy != NULL);
    assert(proxy->network.rx_pkg_size == ping_data_size);

    struct msg_header_t* header = (struct msg_header_t*) proxy->network.rx_buffer;
    assert(header->msg_prefix == prefix_srvPingRep);
    assert(header->msg_index  == 0);

    uint8_t* rx_data = proxy->network.rx_buffer + sizeof(struct msg_header_t);

    struct sockaddr_in* sock_address = (struct sockaddr_in*) &(proxy->network.to_address);
    int ret = memcmp(rx_data, &(sock_address->sin_addr), sizeof(struct in_addr));
    if (ret != 0) {
        LOG_WARNING(proxy, "net_recv_ping(): incorrect ping data - exist(%u.%u.%u.%u), expected(%04x)"
            , rx_data[0], rx_data[1], rx_data[2], rx_data[3]
            , sock_address->sin_addr.s_addr);
    }

    net_ping_update_timer(proxy);
}

void net_ping_update_timer(struct pty2udp_proxy* proxy) {
    // enabling periodic module polling
    proxy->network.ping.send.count = 0;
    int ret = evtimer_del(proxy->network.ping.recv.ev);
    // TODO: here you need to process 'ret'

    ret = evtimer_add(proxy->network.ping.send.ev, &(proxy->network.ping.timeout));
    // TODO: here you need to process 'ret'
}

void cb_timeout__send_ping(evutil_socket_t socket, short events, void* arg) {
    assert(arg != NULL);

    struct pty2udp_proxy* proxy = arg;
    // assert(proxy->network.socket == socket);

    int ret = net_send_ping(proxy);
    // TODO: here you need to process 'ret'

    struct sockaddr_in* sock_address = (struct sockaddr_in*) &(proxy->network.to_address);
    uint8_t* ip_address = (uint8_t*) &(sock_address->sin_addr.s_addr);

    if (proxy->network.ping.send.count == 0) {
        // waiting for a response from the esp module
        ret = evtimer_add(proxy->network.ping.recv.ev, &(proxy->network.ping.recv.timeout));
        // TODO: here you need to process 'ret'

        LOG_DEBUG(proxy, "send ping message to %u.%u.%u.%u"
            , ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    } else {
        LOG_WARNING(proxy, "send ping message to %u.%u.%u.%u (%u)"
            , ip_address[0], ip_address[1], ip_address[2], ip_address[3]
            , (proxy->network.ping.send.count + 1));
    }

    proxy->network.ping.send.count++;
}

void cb_timeout__recv_ping(evutil_socket_t socket, short events, void* arg) {
    assert(arg != NULL);

    struct pty2udp_proxy* proxy = arg;
    // assert(proxy->network.socket == socket);

    LOG_CRITICAL(proxy, "cb_timeout__recv_ping(): !!! connection lost !!!");
    network_finish(proxy);
    // ??? serial_finish(proxy);

    network_init(proxy);
}

