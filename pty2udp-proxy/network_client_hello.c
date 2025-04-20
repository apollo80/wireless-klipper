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

static const size_t client_hello_data_size = sizeof(struct msg_header_t) + sizeof(struct in_addr);


int net_send_client_hello(struct pty2udp_proxy* proxy) {
    if (NULL == proxy)
        return EXIT_FAILURE;

    uint8_t tx_buffer[client_hello_data_size];
    struct msg_header_t* header = (struct msg_header_t*) tx_buffer;
    uint8_t* tx_data = tx_buffer + sizeof(struct msg_header_t);

    header->msg_prefix = prefix_clientHello;
    header->msg_index  = 0;
    header->msg_size   = htons(sizeof(struct in_addr));

    struct sockaddr_in* sock_address = (struct sockaddr_in*) &(proxy->network.to_address);
    memcpy(tx_data, &(sock_address->sin_addr), sizeof(struct in_addr));

    int ret = sendto(proxy->network.socket, tx_buffer, client_hello_data_size
                    , 0 , &(proxy->network.to_address), sizeof(proxy->network.to_address));
    if (ret == -1) {
        LOG_CRITICAL(proxy, "net_send_client_hello(): failed sendto() - %s", strerror(errno));
        return EXIT_FAILURE;
    }
    LOG_DEBUG(proxy, "send client hello message");

    ret = evtimer_add(proxy->network.client_hello.ev_timeout, &(proxy->network.client_hello.timeout));
    // TODO: here you need to process 'ret'

    return EXIT_SUCCESS;
}

void net_recv_server_hello(struct pty2udp_proxy* proxy) {
    assert(proxy != NULL);
    assert(proxy->network.rx_pkg_size == client_hello_data_size);

    struct msg_header_t* header = (struct msg_header_t*) proxy->network.rx_buffer;
    assert(header->msg_prefix == prefix_serverHello);
    assert(header->msg_index  == 0);

    uint8_t* rx_data = proxy->network.rx_buffer + sizeof(struct msg_header_t);

    struct sockaddr_in* sock_address = (struct sockaddr_in*) &(proxy->network.to_address);
    int ret = memcmp(rx_data, &(sock_address->sin_addr), sizeof(struct in_addr));
    if (ret != 0) {
        LOG_WARNING(proxy, "net_recv_server_hello(): incorrect ping data - exist(%x.%x.%x.%x), expected(%04x)"
                    , rx_data[0], rx_data[1], rx_data[2], rx_data[3]
                    , sock_address->sin_addr.s_addr);
    }

    // a confirmation has been received from the esp module
    // this means that the pty can be initialized
    serial_init(proxy);

    proxy->msg_index_klipper = 0;
    proxy->msg_index_mcu = 0;

    // disabling the resending of 'client_hello'
    evtimer_del(proxy->network.client_hello.ev_timeout);

    // enabling periodic module polling
    ret = evtimer_add(proxy->network.ping.send.ev, &(proxy->network.ping.timeout));
    // TODO: here you need to process 'ret'
}

void cb_timeout__client_hello(evutil_socket_t socket, short events, void* arg) {
    assert(arg != NULL);

    struct pty2udp_proxy* proxy = arg;
    // assert(proxy->network.socket == socket);

    net_send_client_hello(proxy);
}
