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
#include <unistd.h>


static void serial_process_next(struct pty2udp_proxy* proxy);

void cb__serial_recv(evutil_socket_t serial_fd, short events, void* args) {
    assert(args != NULL);

    struct pty2udp_proxy* proxy = args;
    assert(serial_fd == proxy->serial.fd_master);

    LOG_DEBUG(proxy, "pty -> net: --->>");

    uint8_t *data_offset = proxy->serial.rx_buffer;
    uint8_t *buff_offset = data_offset + proxy->serial.rx_data_size;
    size_t   buff_size   = sizeof(proxy->serial.rx_buffer) - sizeof(struct msg_header_t) - proxy->serial.rx_data_size;

    ssize_t nread = read(proxy->serial.fd_master, buff_offset, buff_size);
    if(nread < 0) {
        LOG_CRITICAL(proxy, "pty -> net: read() return %zi - %i - %s", nread, errno, strerror(errno));
        event_base_loopexit(proxy->ev_loop, NULL);
        return;
    }

    if(nread == 0) {
        LOG_ERROR(proxy, "pty -> net: read zero bytes - %s", strerror(errno));
        // TODO: reinit_serial(proxy);
        event_base_loopexit(proxy->ev_loop, NULL);
        return;
    }

    proxy->serial.rx_data_size += nread;
    LOG_DEBUG(proxy, "pty -> net: serial receive %zi bytes (+ %zi)", proxy->serial.rx_data_size, nread);
    LOG_DEBUG(proxy, "pty -> net: serial[%zu] += %s"
        , proxy->serial.rx_data_size, array2hex(proxy->serial.rx_buffer, proxy->serial.rx_data_size));


    static const uint8_t stk500v2_leave[] = { 0x1b, 0x01, 0x00, 0x01, 0x0e, 0x11, 0x04 };
    size_t data_size = proxy->serial.rx_data_size;
    if (data_size >= sizeof(stk500v2_leave)
        && 0 == memcmp(stk500v2_leave, data_offset, sizeof(stk500v2_leave)))
    {
        LOG_DEBUG(proxy, "pty -> net: detected stk500v2_leave sequence - skip");
        memcpy(data_offset, data_offset + sizeof(stk500v2_leave), data_size - sizeof(stk500v2_leave));
        data_size -= sizeof(stk500v2_leave);
        proxy->serial.rx_data_size = data_size;
    }

    serial_process_next(proxy);
}

void net_recv_klipper_data_confirm(struct pty2udp_proxy* proxy) {
    struct msg_header_t* header = (struct msg_header_t*) proxy->network.rx_buffer;

    if (header->msg_index != proxy->msg_index_klipper) {
        LOG_DEBUG(proxy, "pty <- net: recv confirm msg - net index - %u; store_netIndex %u - skip"
            , header->msg_index, proxy->msg_index_klipper);
        return;
    }

    event_remove_timer(proxy->resend.ev_timeout);
    proxy->resend.count = 0;

    LOG_DEBUG(proxy, "pty <- net: recv confirm msg - net index - %u; store_netIndex %u - ok"
        , header->msg_index, proxy->msg_index_klipper);

    proxy->msg_index_klipper = (proxy->msg_index_klipper + 1) % (UINT8_MAX + 1);
    LOG_DEBUG(proxy, "pty <- net: net index ->  %u", proxy->msg_index_klipper);

    // shift data
    uint16_t send_data_size = proxy->serial.send_pkg_size - sizeof(struct msg_header_t);
    memmove(proxy->serial.rx_buffer, proxy->serial.rx_buffer + send_data_size, proxy->serial.rx_data_size - send_data_size);

    // correct data size
    proxy->serial.rx_data_size -= send_data_size;
    proxy->serial.send_pkg_size -= send_data_size;

    if (proxy->serial.rx_data_size) {
        LOG_DEBUG(proxy, "pty -> net: serial remain %zi bytes (- %u)", proxy->serial.rx_data_size, send_data_size);
        LOG_DEBUG(proxy, "pty -> net: serial[%zu] - %s"
            , proxy->serial.rx_data_size, array2hex(proxy->serial.rx_buffer, proxy->serial.rx_data_size));
    }

    // update ping
    // net_ping_update_timer(proxy);

    // process next data
    serial_process_next(proxy);
}

void net_recv_mcu_data(struct pty2udp_proxy* proxy) {
    struct msg_header_t* header = (struct msg_header_t*) proxy->network.rx_buffer;
    uint16_t msg_size = ntohs(header->msg_size);

    // check if this message is a duplicate
    uint8_t stored_mcu_index = proxy->msg_index_mcu;
    uint8_t recv_mcu_index = header->msg_index;
    if (stored_mcu_index != recv_mcu_index) {

        if (stored_mcu_index == (recv_mcu_index + 1)) {
            // the previous packet has been received, which has already been recorded in serial.
            // that is, the mcu has not received our previous confirmation, we duplicate it
            union uni_header_t confirm_msg;
            confirm_msg.header.msg_prefix = prefix_mcuDataConfirm;
            confirm_msg.header.msg_index = recv_mcu_index;
            confirm_msg.header.msg_size   = 0;

            ssize_t send_bytes = sendto(proxy->network.socket
                    , confirm_msg.raw, sizeof(confirm_msg.raw)
                    , MSG_DONTWAIT
                    , &(proxy->network.to_address), sizeof(proxy->network.to_address));
            assert(send_bytes == sizeof(confirm_msg.raw));
            LOG_DEBUG(proxy, "pty <- net: send old uart_confirm msg: mcu index - %u;", confirm_msg.header.msg_index);
            return;
        }

        LOG_DEBUG(proxy, "pty <- net: recv data msg - mcu_index(%u), stored mcu index (%u) - duplicate; skip"
            , recv_mcu_index, stored_mcu_index);
        return;
    }

    // send confirmation
    {
        union uni_header_t confirm_msg;
        confirm_msg.header.msg_prefix = prefix_mcuDataConfirm;
        confirm_msg.header.msg_index = header->msg_index;
        confirm_msg.header.msg_size   = 0;

        ssize_t send_bytes = sendto(proxy->network.socket
                , confirm_msg.raw, sizeof(confirm_msg.raw)
                , MSG_DONTWAIT
                , &(proxy->network.to_address), sizeof(proxy->network.to_address));
        assert(send_bytes == sizeof(confirm_msg.raw));
        LOG_DEBUG(proxy, "pty <- net: send uart_confirm msg: mcu index - %u;", header->msg_index);
    }

    // send data to serial (pty)
    uint8_t *data_offset = proxy->network.rx_buffer + sizeof(struct msg_header_t);
    LOG_DEBUG(proxy, "pty <- net: network[%u] - %s", msg_size, array2hex(data_offset, msg_size));

    event_remove_timer(proxy->resend.ev_timeout);
    proxy->resend.count = 0;

    ssize_t write_bytes = write(proxy->serial.fd_master, proxy->network.rx_buffer + sizeof(struct msg_header_t), msg_size);
    assert(write_bytes == msg_size);

    LOG_DEBUG(proxy, "pty <- net: write to serial: %zi bytes;", write_bytes);
    proxy->msg_index_mcu = (recv_mcu_index + 1) % (UINT8_MAX + 1);

    // update ping
    // net_ping_update_timer(proxy);

    // process next data ??
    serial_process_next(proxy);
}

void serial_process_next(struct pty2udp_proxy* proxy) {

    const size_t max_pkg_size = 119;

    uint8_t *data = proxy->serial.rx_buffer;
    size_t  data_size = proxy->serial.rx_data_size;

    size_t  msg_start = 0;
    uint8_t next_pkg_size = 0;
    uint8_t pkg_count = 0;
    while (data_size > msg_start) {
        if (data[msg_start] == 0x7e) {
            // LOG_DEBUG(proxy, "pty -> net:     found sync-byte in position %zu - skip it", msg_start);
            msg_start++;
            continue;
        }

        next_pkg_size = data[msg_start];
        if (msg_start + next_pkg_size > max_pkg_size)
            break;

        msg_start += next_pkg_size;
        pkg_count++;
    }
    uint16_t pkg_size = msg_start;


    if (pkg_count == 0) {
        if (pkg_size + next_pkg_size > max_pkg_size) {
            LOG_CRITICAL(proxy, "pty -> net: serial_process_next() - pkg_size = %u, next_pkg_size = %u", pkg_size, next_pkg_size);
            // ????
        }

        LOG_DEBUG(proxy, "pty -> net: need more data, waiting ...");
        event_add(proxy->serial.ev, NULL);
        return;
    }

    // create package
    {
        struct msg_header_t *header = (struct msg_header_t *) proxy->serial.send_pkg;
        uint16_t * crc16_offset = (uint16_t *) (proxy->serial.send_pkg + sizeof(struct msg_header_t) + pkg_size);

        header->msg_prefix = prefix_klipperData;
        header->msg_index = proxy->msg_index_klipper;
        header->msg_size = htons(pkg_size);

        memcpy(proxy->serial.send_pkg + sizeof(struct msg_header_t), proxy->serial.rx_buffer, pkg_size);

        proxy->serial.send_pkg_size = sizeof(struct msg_header_t) + pkg_size;
        LOG_DEBUG(proxy, "pty -> net: create pkg - size %zu, msg_index %u, data size = %u"
        , proxy->serial.send_pkg_size
        , header->msg_index
        , ntohs(header->msg_size));

        LOG_DEBUG(proxy, "pty -> net: %s", array2hex(proxy->serial.send_pkg, proxy->serial.send_pkg_size));
    }

    // send package
    net_send_klipper_data(proxy);

    // wait confirm message
    event_add(proxy->resend.ev_timeout, &(proxy->resend.timeout));
}
