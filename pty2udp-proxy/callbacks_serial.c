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
#include <stdbool.h>
#include <unistd.h>

static bool verify_input_data(const uint8_t* data, size_t data_size);
static uint16_t crc16_ccitt(uint8_t *buf, uint_fast8_t len);

void cb_proxy__serial_recv(evutil_socket_t serial_fd, short events, void* args) {
    struct pty2udp_proxy* proxy = args;
    assert(serial_fd == proxy->serial_fd);

    LOG_DEBUG(proxy, "pty -> net: --->>");

    if (!proxy->msg_net_confirmed) {
        LOG_DEBUG(proxy, "pty -> net: prev msg is not confirmed - waiting ...");
        return;
    }

    uint8_t *data_offset = proxy->serial_rx + sizeof(struct msg_header_t);
    uint8_t *buff_offset = data_offset + proxy->serial_data_size;
    size_t   buff_size   = sizeof(proxy->serial_rx) - sizeof(struct msg_header_t) - proxy->serial_data_size;

    ssize_t nread = read(proxy->serial_fd, buff_offset, buff_size);
    if(nread < 0) {
        LOG_CRITICAL(proxy, "pty -> net: read() return %zi - %i - %s", nread, errno, strerror(errno));
        event_base_loopexit(proxy->event_base, NULL);
        return;
    }

    if(nread == 0) {
        LOG_ERROR(proxy, "pty -> net: read zero bytes - %s", strerror(errno));
        // reinit_serial(proxy);
        event_base_loopexit(proxy->event_base, NULL);
        return;
    }

    proxy->serial_data_size += nread;
    LOG_DEBUG(proxy, "pty -> net: serial receive msg block %zi bytes (+ %zi)", proxy->serial_data_size, nread);
    LOG_DEBUG(proxy, "pty -> net: serial[%zu] - %s", proxy->serial_data_size, array2hex(data_offset, proxy->serial_data_size));

    // verify input data
    {

        static const uint8_t stk500v2_leave[] = { 0x1b, 0x01, 0x00, 0x01, 0x0e, 0x11, 0x04 };
        size_t data_size = proxy->serial_data_size;
        if (data_size >= sizeof(stk500v2_leave)
                && 0 == memcmp(stk500v2_leave, data_offset, sizeof(stk500v2_leave)))
        {
            LOG_DEBUG(proxy, "pty -> net: detected stk500v2_leave sequence - skip");
            memcpy(data_offset, data_offset + sizeof(stk500v2_leave), data_size - sizeof(stk500v2_leave));
            data_size -= sizeof(stk500v2_leave);
            proxy->serial_data_size = data_size;
        }

        if (data_size == 0)
            return;

        bool need_more = verify_input_data(data_offset, data_size);
        if (need_more) {
            return;
        }
    }

    // create udp package
    struct msg_header_t* header = (struct msg_header_t*) proxy->serial_rx;

    if (0 == proxy->session_start) {
        proxy->session_start = 1;
        header->msg_prefix = htonl(prefix_netStart);
    } else {
        header->msg_prefix = htonl(prefix_netData);
    }

    header->net_index  = htonl(proxy->msg_net_index);
    header->uart_index = htonl(proxy->msg_uart_index);
    header->msg_size   = htonl(proxy->serial_data_size);

    ssize_t nwrite = sendto(proxy->socket_fd
           , proxy->serial_rx, proxy->serial_data_size + sizeof(struct msg_header_t)
           , MSG_DONTWAIT
           , &(proxy->server_address), sizeof(proxy->server_address));

    assert(nwrite == (proxy->serial_data_size + sizeof(struct msg_header_t)));
    proxy->msg_net_confirmed = false;

#ifndef NDEBUG
    char address_as_str[128];
    struct sockaddr_in* tmp = (struct sockaddr_in*) &(proxy->server_address);

    inet_ntop(AF_INET, &(tmp->sin_addr), address_as_str, sizeof(address_as_str));
    LOG_DEBUG(proxy, "pty -> net: send %zi (= %zi + %zi) bytes to %s:%u, net index %u, prefix 0x%0x"
            , nwrite, proxy->serial_data_size, sizeof(struct msg_header_t)
            , address_as_str, ntohs(tmp->sin_port)
            , ntohl(header->net_index), ntohl(header->msg_prefix));
#endif

    // proxy->serial_data_size = 0;
    event_add(proxy->resend_timeout_event, &(proxy->resend_timeout));
}

bool verify_input_data(uint8_t const* data, size_t data_size) {
    // LOG_DEBUG(proxy, "pty -> net: verify input data (%lu bytes) ...", data_size);

    size_t  msg_start = 0;
    size_t  pkg_count = 0;
    while (data_size > msg_start) {
        if (data[msg_start] == 0x7e) {
            // LOG_DEBUG(proxy, "pty -> net:     found sync-byte in position %zu - skip it", msg_start);
            msg_start++;
            continue;
        }

        uint8_t pkg_size = data[msg_start];
        pkg_count++;
        // LOG_DEBUG(proxy, "pty -> net:     found pkg - %zu; msg_start %zu, pkg_size %u", pkg_count, msg_start, pkg_size);
        msg_start += pkg_size;
    }
    bool need_more = (data_size < msg_start);

    // LOG_DEBUG(proxy, "pty -> net: ... verified - %s", (need_more ? "need more" : "is ok"));
    return need_more;
}

uint16_t crc16_ccitt(uint8_t *buf, uint_fast8_t len) {
    uint16_t crc = 0xffff;
    while (len--) {
        uint8_t data = *buf++;
        data ^= crc & 0xff;
        data ^= data << 4;
        crc = ((((uint16_t)data << 8) | (crc >> 8)) ^ (uint8_t)(data >> 4)
               ^ ((uint16_t)data << 3));
    }
    return crc;
}
