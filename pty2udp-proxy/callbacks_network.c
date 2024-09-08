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
#include <stdbool.h>
#include <string.h>

static void send_confirmation(struct pty2udp_proxy* proxy);
static bool is_dublicate_uart_msg(struct pty2udp_proxy* proxy);

void cb_proxy__network_recv(evutil_socket_t socket_fd, short events, void* args) {
    struct pty2udp_proxy* proxy = args;
    assert(socket_fd == proxy->socket_fd);


    ssize_t nread = recvfrom(socket_fd
            , proxy->socket_rx, sizeof(proxy->socket_rx)
            , 0
            , NULL, 0);
    if (nread < 0) {
        LOG_CRITICAL(proxy, "pty <- net: read() return %zi: %s", nread, strerror(errno));
        exit(1);
    }

    if(nread == 0) {
        LOG_CRITICAL(proxy, "pty <- net: read zero bytes");
        exit(1);
    }

    struct msg_header_t* header = (struct msg_header_t*) proxy->socket_rx;

    header->msg_prefix = ntohl(header->msg_prefix);
    header->net_index  = ntohl(header->net_index);
    header->uart_index = ntohl(header->uart_index);
    header->msg_size   = ntohl(header->msg_size);


    if (header->msg_prefix == prefix_udpLog) {

        proxy->socket_rx[sizeof(struct msg_header_t) + header->msg_size] = 0;
        LOG_INFO(proxy, "%s", proxy->socket_rx + sizeof(struct msg_header_t));

    } else if (header->msg_prefix == prefix_netConfirm) {

        LOG_DEBUG(proxy, "pty <- net: <<---");

        if (header->net_index == proxy->msg_net_index) {
            event_remove_timer(proxy->resend_timeout_event);
            proxy->resend_count = 0;

            LOG_DEBUG(proxy, "pty <- net: recv confirm msg - net index - %u; uart index - %u; store_netIndex %u - ok"
                    , header->net_index, header->uart_index, proxy->msg_net_index);

            proxy->msg_net_index++;
            if (proxy->msg_net_index == (UINT8_MAX + 1)) {
                proxy->msg_net_index %= (UINT8_MAX + 1);
                LOG_DEBUG(proxy, "pty <- net: net index ->  %u", proxy->msg_net_index);
            }

            proxy->msg_net_confirmed = true;
            proxy->serial_data_size = 0;
        } else {
            LOG_DEBUG(proxy, "pty <- net: recv confirm msg - net index - %u; uart index - %u; store_netIndex %u - skip"
                , header->net_index, header->uart_index, proxy->msg_net_index);
        }

    } else if (header->msg_prefix == prefix_uartData) {

        LOG_DEBUG(proxy, "pty <- net: <<---");
        LOG_DEBUG(proxy, "pty <- net: recv uart_data msg: net index - %u; uart index - %u; data size - %u"
                , header->net_index, header->uart_index, header->msg_size);

        // send confirmation
        send_confirmation(proxy);

        // checking whether this message is a duplicate
        if (is_dublicate_uart_msg(proxy))
            return;

        proxy->msg_uart_index = header->uart_index;

        // if the confirmation message is lost, but the data message is received
        if (header->net_index == proxy->msg_net_index) {
            event_remove_timer(proxy->resend_timeout_event);
            proxy->resend_count = 0;

            LOG_DEBUG(proxy, "pty <- net: recv data msg confirm -  net index - %u; uart index - %u; store_netIndex %u - ok"
                    , header->net_index, header->uart_index, proxy->msg_net_index);

            proxy->msg_net_index++;
            if (proxy->msg_net_index == (UINT8_MAX + 1)) {
                proxy->msg_net_index %= (UINT8_MAX + 1);
                LOG_DEBUG(proxy, "pty <- net: net index ->  %u", proxy->msg_net_index);
            }

            proxy->msg_net_confirmed = true;
            proxy->serial_data_size = 0;
        }

        uint8_t *data_offset = proxy->socket_rx + sizeof(struct msg_header_t);
        LOG_DEBUG(proxy, "pty <- net: network[%u] - %s", header->msg_size, array2hex(data_offset, header->msg_size));

        ssize_t nwrite = write(proxy->serial_fd, proxy->socket_rx + sizeof(struct msg_header_t), header->msg_size);
        assert(nwrite == header->msg_size);

        LOG_DEBUG(proxy, "pty <- net: write to serial: %zi bytes;", nwrite);

    } else {
        LOG_ERROR(proxy, "pty <- net: <<--->>");
        LOG_ERROR(proxy, "pty <- net: incorrect msg prefix = 0x{%x}; waiting prefix 0x{%x}"
                , header->msg_prefix, prefix_uartData);
        LOG_ERROR(proxy, "pty <- net: <<--->>");
    }

    // event_remove_timer(proxy->session_timeout_event);
    // event_add(proxy->session_timeout_event, &(proxy->session_timeout));
}

void send_confirmation(struct pty2udp_proxy* proxy) {
    struct msg_header_t* header = (struct msg_header_t*) proxy->socket_rx;

    union uni_header_t confirm_msg;
    confirm_msg.header.msg_prefix = htonl(prefix_uartConfirm);
    confirm_msg.header.net_index  = htonl(header->net_index);
    confirm_msg.header.uart_index = htonl(header->uart_index);
    confirm_msg.header.msg_size   = 0;

    ssize_t nwrite = sendto(proxy->socket_fd
            , confirm_msg.raw, sizeof(confirm_msg.raw)
            , MSG_DONTWAIT
            , &(proxy->server_address), sizeof(proxy->server_address));
    assert(nwrite == sizeof(confirm_msg.raw));

    LOG_DEBUG(proxy, "pty <- net: send uart_confirm msg: net index - %u; uart index - %u;"
            , header->net_index, header->uart_index);
}

bool is_dublicate_uart_msg(struct pty2udp_proxy* proxy) {
    if (proxy->first_uart_message) {
        proxy->first_uart_message = false;
        return false;
    };

    struct msg_header_t* header = (struct msg_header_t*) proxy->socket_rx;

    uint16_t stored_uart_index = proxy->msg_uart_index;
    uint16_t recv_uart_index = header->uart_index;

    // correction for intersections in the range of 2^8
    if (stored_uart_index > 250 && recv_uart_index < 5) {
        recv_uart_index += (UINT8_MAX + 1);
    }

    if (recv_uart_index <= stored_uart_index) {
        LOG_DEBUG(proxy, "pty <- net: recv data msg - uart_index(%u) - duplicate", header->uart_index);
        return true;
    }
    return false;
}
