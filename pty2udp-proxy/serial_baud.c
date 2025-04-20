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

#include <asm/termios.h>
#include <string.h>

// forward declaration
extern int ioctl (int __fd, unsigned long int __request, ...) __THROW;

int set_serial_speed_custom(struct pty2udp_proxy* proxy) {
    // set non standard speed
    struct termios2 tio;

    // ioctl();
    int ret = ioctl(proxy->serial.fd_master, TCGETS2, &tio);
    if (ret != 0) {
        LOG_CRITICAL(proxy, "failed ioctl(TCGETS2): %s", strerror(errno));
        return EXIT_FAILURE;
    }
    tio.c_cflag &= ~CBAUD;
    tio.c_cflag |= BOTHER;
    tio.c_ispeed = proxy->serial.baud;
    tio.c_ospeed = proxy->serial.baud;

    ret = ioctl(proxy->serial.fd_master, TCSETS2, &tio);
    if (ret != 0) {
        LOG_CRITICAL(proxy, "failed ioctl(TCSETS2): %s", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
