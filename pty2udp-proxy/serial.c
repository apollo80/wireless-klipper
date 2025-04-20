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
#include <fcntl.h>
#include <pty.h>
#include <string.h>
#include <unistd.h>


/// @brief
/// @param[in] serial_fd
/// @param[in] serial_baud
/// @return
static int set_serial_speed(struct pty2udp_proxy* proxy);

/// @brief
/// @param[in] serial_fd
/// @param[in] serial_baud
int set_serial_speed_custom(struct pty2udp_proxy* proxy);


int serial_init(struct pty2udp_proxy* proxy) {

    struct termios serial_tio;
    memset(&serial_tio, 0, sizeof(serial_tio));

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
            return EXIT_FAILURE;
        }
        proxy->serial.fd_master = serial_fd_master;
        proxy->serial.fd_slave = serial_fd_slave;
    }
    set_serial_speed(proxy);

    // create symbol link
    int ret = symlink(dev_path, proxy->serial.path);
    if (ret) {
        LOG_WARNING(proxy, "failed create link - \'%s\' -> \'%s\': %s", proxy->serial.path, dev_path, strerror(errno));
        ret = unlink(proxy->serial.path);
        if (ret) {
            LOG_CRITICAL(proxy, "failed unlink file (\'%s\'): %s", proxy->serial.path, strerror(errno));
            free(dev_path);
            return EXIT_FAILURE;
        }

        ret = symlink(dev_path, proxy->serial.path);
        if (ret) {
            LOG_CRITICAL(proxy, "failed create link - \'%s\' -> \'%s\': %s", proxy->serial.path, dev_path, strerror(errno));
            free(dev_path);
            return EXIT_FAILURE;
        }
    }
    LOG_INFO(proxy, "create device link - \'%s\' -> \'%s\'", proxy->serial.path, dev_path);
    free(dev_path);

    int fd_flags = fcntl(proxy->serial.fd_master, F_GETFL, 0);
    fcntl(proxy->serial.fd_master, F_SETFL, fd_flags | O_NONBLOCK);

    struct event* serial_event = event_new(proxy->ev_loop, proxy->serial.fd_master, EV_READ, cb__serial_recv, proxy);
    if (NULL == serial_event) {
        LOG_CRITICAL(proxy, "failed event_new(serial) - %s", strerror(errno));

        close(proxy->serial.fd_master);
        proxy->serial.fd_master = -1;

        return EXIT_FAILURE;
    }
    proxy->serial.ev = serial_event;

    ret = event_add(proxy->serial.ev, NULL);
    // TODO: if return error -> ???

    return EXIT_SUCCESS;
}

int serial_finish(struct pty2udp_proxy* proxy) {
    // serial event
    if (proxy->serial.ev) {
        event_free(proxy->serial.ev);
        proxy->serial.ev = NULL;
    }

    // remove link
    unlink(proxy->serial.path);

    // close slave fd
    close(proxy->serial.fd_slave);
    proxy->serial.fd_slave = -1;

    // close master fd
    close(proxy->serial.fd_master);
    proxy->serial.fd_master = -1;
}

int set_serial_speed(struct pty2udp_proxy* proxy) {
    int default_serial_baud = B0;

    switch (proxy->serial.baud) {

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
    int ret = tcgetattr(proxy->serial.fd_master, &term_settings);
    if (ret != 0) {
        LOG_CRITICAL(proxy, "tcgetattr() error: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    ret = cfsetispeed(&term_settings, default_serial_baud);
    if (ret != 0) {
        LOG_CRITICAL(proxy, "cfsetispeed() error: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    ret = cfsetospeed(&term_settings, default_serial_baud);
    if (ret != 0) {
        LOG_CRITICAL(proxy, "cfsetospeed() error: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    ret = tcsetattr(proxy->serial.fd_master, TCSANOW, &term_settings);
    if (ret != 0) {
        LOG_CRITICAL(proxy, "tcsetattr() error: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

