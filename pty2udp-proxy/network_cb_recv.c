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
#include <errno.h>
#include <string.h>
#include <unistd.h>


static void send_confirmation(struct pty2udp_proxy* proxy);
static int is_dublicate_mcu_msg(struct pty2udp_proxy* proxy);


void cb__udp_recv(evutil_socket_t socket, short events, void* arg) {
    assert(arg != NULL);

    struct pty2udp_proxy* proxy = arg;
    assert(proxy->network.socket == socket);

    struct sockaddr recv_ip_address;
    uint8_t* recv_ip_address_array = (uint8_t*) &(((struct sockaddr_in*) &recv_ip_address)->sin_addr.s_addr);
    socklen_t recv_ip_address_len = sizeof(recv_ip_address);

    ssize_t nread = recvfrom(proxy->network.socket
            , proxy->network.rx_buffer, sizeof(proxy->network.rx_buffer)
            , 0
            , &recv_ip_address, &recv_ip_address_len);

    if (nread < 0) {
        LOG_CRITICAL(proxy, "pty <- net: read() return %zi: %s", nread, strerror(errno));
        exit(1);
    }

    if(nread == 0) {
        LOG_CRITICAL(proxy, "pty <- net: read zero bytes");
        exit(1);
    }

    proxy->network.rx_pkg_size = nread;
    struct msg_header_t* header = (struct msg_header_t*) proxy->network.rx_buffer;
    uint16_t msg_size = ntohs(header->msg_size);


    if (header->msg_prefix == prefix_udpLog) {

        proxy->network.rx_buffer[sizeof(struct msg_header_t) + msg_size] = 0;
        LOG_INFO(proxy, "%s", proxy->network.rx_buffer + sizeof(struct msg_header_t));

    } else if (header->msg_prefix == prefix_serverHello) {
        LOG_DEBUG(proxy, "recv server hello from %u.%u.%u.%u"
            , recv_ip_address_array[0], recv_ip_address_array[1], recv_ip_address_array[2], recv_ip_address_array[3]);
        net_recv_server_hello(proxy);

    } else if (header->msg_prefix == prefix_srvPingRep) {
        LOG_DEBUG(proxy, "recv ping response from from %u.%u.%u.%u"
            , recv_ip_address_array[0], recv_ip_address_array[1], recv_ip_address_array[2], recv_ip_address_array[3]);
        net_recv_ping(proxy);

    } else if (header->msg_prefix == prefix_klipperDataConfirm) {
        LOG_DEBUG(proxy, "pty <- net: <<--- klipper data confirm from %u.%u.%u.%u"
            , recv_ip_address_array[0], recv_ip_address_array[1], recv_ip_address_array[2], recv_ip_address_array[3]);
        net_recv_klipper_data_confirm(proxy);

    } else if (header->msg_prefix == prefix_mcuData) {
        LOG_DEBUG(proxy, "pty <- net: <<--- mcu data, uart index - %u; data size - %u; from %u.%u.%u.%u"
            , header->msg_index, msg_size
            , recv_ip_address_array[0], recv_ip_address_array[1], recv_ip_address_array[2], recv_ip_address_array[3]);
        net_recv_mcu_data(proxy);

    } else {
        LOG_ERROR(proxy, "pty <- net: <<--->>");
        LOG_ERROR(proxy, "pty <- net: incorrect msg prefix = 0x%02x;", header->msg_prefix);
        LOG_ERROR(proxy, "pty <- net: <<--->>");
    }

    // event_remove_timer(proxy->session_timeout_event);
    // event_add(proxy->session_timeout_event, &(proxy->session_timeout));
}

void send_confirmation(struct pty2udp_proxy* proxy) {
    struct msg_header_t* header = (struct msg_header_t*) proxy->network.rx_buffer;

    union uni_header_t confirm_msg;
    confirm_msg.header.msg_prefix = prefix_mcuDataConfirm;
    confirm_msg.header.msg_index = header->msg_index;
    confirm_msg.header.msg_size   = 0;

    ssize_t nwrite = sendto(proxy->network.socket
            , confirm_msg.raw, sizeof(confirm_msg.raw)
            , MSG_DONTWAIT
            , &(proxy->network.to_address), sizeof(proxy->network.to_address));
    assert(nwrite == sizeof(confirm_msg.raw));

    LOG_DEBUG(proxy, "pty <- net: send uart_confirm msg: net index - %u;", header->msg_index);
}

int is_dublicate_mcu_msg(struct pty2udp_proxy* proxy) {
    struct msg_header_t* header = (struct msg_header_t*) proxy->network.rx_buffer;

    uint16_t stored_uart_index = proxy->msg_index_mcu;
    uint16_t recv_uart_index = header->msg_index;

    // correction for intersections in the range of 2^8
    if (stored_uart_index > 250 && recv_uart_index < 5) {
        recv_uart_index += (UINT8_MAX + 1);
    }

    if (recv_uart_index <= stored_uart_index) {
        LOG_DEBUG(proxy, "pty <- net: recv data msg - mcu_index(%u) - duplicate", header->msg_index);
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
