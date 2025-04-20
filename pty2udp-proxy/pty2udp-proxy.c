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


#define PTY2UDP_PROXY_MAX   128
static size_t pty2udp_proxy_size = 0;
static struct pty2udp_proxy proxy_list[PTY2UDP_PROXY_MAX];

struct pty2udp_proxy*  pty2udp_proxy_new() {
    if (pty2udp_proxy_size > PTY2UDP_PROXY_MAX)
        return NULL;

    struct pty2udp_proxy* ret_value = &(proxy_list[pty2udp_proxy_size]);
    memset(ret_value, 0, sizeof(struct pty2udp_proxy));
    // default settings


    // serial
    ret_value->serial.fd_master = -1;
    ret_value->serial.fd_slave = -1;

    ret_value->serial.path[0] = 0;
    ret_value->serial.baud = 250000;
    ret_value->serial.rx_data_size = 0;
    ret_value->serial.send_pkg_size = 0;
    ret_value->serial.ev = NULL;

    ret_value->network.to_name[0] = 0;
    ret_value->network.to_port = 8888;
    ret_value->network.ev_recv = NULL;

    // network
    ret_value->network.client_hello.ev_timeout = NULL;
    ret_value->network.client_hello.timeout.tv_sec = 1;
    ret_value->network.client_hello.timeout.tv_usec = 0;

    ret_value->network.rx_pkg_size = 0;

    // resend
    ret_value->resend.ev_timeout = NULL;
    ret_value->resend.timeout.tv_sec = 0;
    ret_value->resend.timeout.tv_usec = 100000;
    ret_value->resend.count = 0;
    ret_value->resend.count = 10;

    ret_value->log.level = TRACE;

    pty2udp_proxy_size++;
    return ret_value;
}

size_t pty2udp_proxy_count() {
    return pty2udp_proxy_size;
}

struct pty2udp_proxy* pty2udp_proxy_get(size_t proxy_index) {
    return (proxy_index < pty2udp_proxy_size ? &(proxy_list[proxy_index]): NULL);
}

struct pty2udp_proxy* pty2udp_proxy_get_by_socket(int socket) {
    struct pty2udp_proxy* ret_value = NULL;
    for(size_t idx = 0; idx < pty2udp_proxy_count(); ++idx) {
        if (proxy_list[idx].network.socket == socket) {
            ret_value = &(proxy_list[idx]);
            break;
        }
    }
    return ret_value;
}

int pty2udp_proxy_init(size_t proxy_index, struct event_base *ev_loop) {
    struct pty2udp_proxy* proxy = pty2udp_proxy_get(proxy_index);
    if (!proxy)
        return EXIT_FAILURE;

    proxy->ev_loop = ev_loop;

    int ret = log_init(proxy);
    if (ret != EXIT_SUCCESS)
        return ret;

    LOG_DEBUG(proxy, "init log succeed");

    ret = network_init(proxy);
    if (ret != EXIT_SUCCESS)
        return ret;

    return EXIT_SUCCESS;
}


void pty2udp_proxy_reset(size_t proxy_index) {
    struct pty2udp_proxy* proxy = pty2udp_proxy_get(proxy_index);
    if (!proxy)
        return;

    // serial
    serial_finish(proxy);

    // network
    network_finish(proxy);


    // resend
    if (proxy->resend.ev_timeout) {
        event_free(proxy->resend.ev_timeout);
        proxy->resend.ev_timeout = 0;
    }
    proxy->resend.count = 0;

    // log
    if (proxy->log.file != NULL) {
        fclose(proxy->log.file);
        proxy->log.file = NULL;
    }
}
