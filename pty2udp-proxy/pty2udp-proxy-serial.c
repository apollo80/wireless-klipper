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

#include <fcntl.h>
#include <pty.h>
#include <string.h>
#include <unistd.h>


/// @brief
/// @param[in] serial_fd
/// @param[in] serial_baud
/// @return
static bool set_serial_speed(struct pty2udp_proxy* proxy);

/// @brief
/// @param[in] serial_fd
/// @param[in] serial_baud
bool set_serial_speed_custom(struct pty2udp_proxy* proxy);


struct event* pty2udp_proxy_init_serial(struct pty2udp_proxy* proxy) {

    struct termios serial_tio;
    serial_tio.c_iflag = 0;
    serial_tio.c_oflag = 0;

    // set to 8N1
    serial_tio.c_cflag &= ~PARENB;
    serial_tio.c_cflag &= ~CSTOPB;
    serial_tio.c_cflag &= ~CSIZE;
    serial_tio.c_cflag |= CS8;

    serial_tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);;
    // serial_tio.c_cc[VMIN] = 1;
    // serial_tio.c_cc[VTIME] = 5;


    char *dev_path = calloc(PATH_MAX, sizeof(char));
    {
        int serial_fd_master = 0;
        int serial_fd_slave = 0;
        int ret = openpty(&serial_fd_master, &serial_fd_slave, dev_path, &serial_tio, NULL);
        if (ret) {
            perror("failed openpty(): ");
            free(dev_path);
            return NULL;
        }
        proxy->serial_fd = serial_fd_master;
        proxy->serial_fd_slave = serial_fd_slave;
    }
    set_serial_speed(proxy);

    // create symbol link
    int ret = symlink(dev_path, proxy->serial_path);
    if (ret) {
        LOG_WARNING(proxy, "failed create link - \'%s\' -> \'%s\': %s", proxy->serial_path, dev_path, strerror(errno));
        ret = unlink(proxy->serial_path);
        if (ret) {
            LOG_CRITICAL(proxy, "failed unlink file (\'%s\'): %s", proxy->serial_path, strerror(errno));
            free(dev_path);
            return NULL;
        }

        ret = symlink(dev_path, proxy->serial_path);
        if (ret) {
            LOG_CRITICAL(proxy, "failed create link - \'%s\' -> \'%s\': %s", proxy->serial_path, dev_path, strerror(errno));
            free(dev_path);
            return NULL;
        }
    }
    LOG_INFO(proxy, "create device link - \'%s\' -> \'%s\'", proxy->serial_path, dev_path);
    free(dev_path);

    int fd_flags = fcntl(proxy->serial_fd, F_GETFL, 0);
    fcntl(proxy->serial_fd, F_SETFL, fd_flags | O_NONBLOCK);

    struct event* serial_event = event_new(proxy->event_base, proxy->serial_fd, EV_READ |EV_PERSIST, cb_proxy__serial_recv, proxy);
    if (NULL == serial_event) {
        close(proxy->serial_fd);
        proxy->serial_fd = -1;

        return NULL;
    }

    proxy->serial_event = serial_event;
    return proxy->serial_event;
}

bool set_serial_speed(struct pty2udp_proxy* proxy) {
    int default_serial_baud = B0;

    switch (proxy->serial_baud) {

#define case_baud(baud)      case baud: default_serial_baud = B ## baud; break;
        case_baud(9600);
        case_baud(19200);
        case_baud(38400);
        case_baud(57600);
        case_baud(115200);
        case_baud(230400);
        case_baud(460800);
        case_baud(500000);
        case_baud(576000);
        case_baud(921600);
        case_baud(1000000);
        case_baud(1152000);
        case_baud(1500000);
        case_baud(2000000);
        case_baud(2500000);
        case_baud(3000000);
        case_baud(3500000);
        case_baud(4000000);
#undef case_baud

        default:
            return set_serial_speed_custom(proxy);
    }

    struct termios term_settings;
    int ret = tcgetattr(proxy->serial_fd, &term_settings);
    if (ret != 0) {
        LOG_CRITICAL(proxy, "tcgetattr() error: %s", strerror(errno));
        return false;
    }

    ret = cfsetispeed(&term_settings, default_serial_baud);
    if (ret != 0) {
        LOG_CRITICAL(proxy, "cfsetispeed() error: %s", strerror(errno));
        return false;
    }
    ret = cfsetospeed(&term_settings, default_serial_baud);
    if (ret != 0) {
        LOG_CRITICAL(proxy, "cfsetospeed() error: %s", strerror(errno));
        return false;
    }

    ret = tcsetattr(proxy->serial_fd, TCSANOW, &term_settings);
    if (ret != 0) {
        LOG_CRITICAL(proxy, "tcsetattr() error: %s", strerror(errno));
        return false;
    }

    return true;
}

