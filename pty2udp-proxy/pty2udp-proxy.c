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


#define PTY2UDP_PROXY_MAX   128
static size_t pty2udp_proxy_size = 0;
static struct pty2udp_proxy proxy_list[PTY2UDP_PROXY_MAX];

struct event* pty2udp_proxy_init_network(struct pty2udp_proxy* proxy);
struct event* pty2udp_proxy_init_serial(struct pty2udp_proxy* proxy);

struct pty2udp_proxy*  pty2udp_proxy_new() {
    if (pty2udp_proxy_size > PTY2UDP_PROXY_MAX)
        return NULL;

    struct pty2udp_proxy* ret_value = (proxy_list + (pty2udp_proxy_size * sizeof(struct pty2udp_proxy)));
    memset(ret_value, 0, sizeof(struct pty2udp_proxy));

    ret_value->socket_fd = -1;
    ret_value->serial_fd = -1;

    // default settings
    ret_value->serial_path[0] = 0;
    ret_value->serial_baud = 250000;
    ret_value->serial_data_size = 0;
    ret_value->serial_event = NULL;

    ret_value->server_name[0] = 0;
    ret_value->server_port = 8888;
    ret_value->network_event = NULL;

    ret_value->log_level = TRACE;

    pty2udp_proxy_size++;
    return ret_value;
}

size_t pty2udp_proxy_count() {
    return pty2udp_proxy_size;
}

struct pty2udp_proxy* pty2udp_proxy_get(size_t proxy_index) {
    return (proxy_index < pty2udp_proxy_size ? (proxy_list + proxy_index * sizeof(struct pty2udp_proxy)) : NULL);
}

struct pty2udp_proxy* pty2udp_proxy_init(size_t proxy_index, struct event_base *base) {
    struct pty2udp_proxy* proxy = pty2udp_proxy_get(proxy_index);
    if (!proxy)
        return proxy;

    proxy->event_base = base;
    if (strlen(proxy->log_path)) {
        proxy->log_file = fopen(proxy->log_path, "w");
    }

    LOG_DEBUG(proxy, "init proxy");

    struct event* serial_ev = pty2udp_proxy_init_serial(proxy);
    struct event* network_ev = pty2udp_proxy_init_network(proxy);

    if (serial_ev == NULL || network_ev == NULL) {
        if (serial_ev)
            event_free(serial_ev);

        if (network_ev)
            event_free(network_ev);

        fclose(proxy->log_file);
        return NULL;
    }

    event_add(proxy->serial_event, NULL);
    event_add(proxy->network_event, NULL);

    return proxy;
}

void pty2udp_proxy_reset(size_t proxy_index) {
    struct pty2udp_proxy* proxy = pty2udp_proxy_get(proxy_index);
    if (!proxy)
        return;

    event_free(proxy->network_event);
    close(proxy->socket_fd);
    close(proxy->serial_fd_slave);

    event_free(proxy->serial_event);
    close(proxy->serial_fd);
    unlink(proxy->serial_path);

    event_free(proxy->resend_timeout_event);

    fclose(proxy->log_file);
}