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

bool set_serial_speed_custom(struct pty2udp_proxy* proxy) {
    // set non standard speed
    struct termios2 tio;

    // ioctl();
    int ret = ioctl(proxy->serial_fd, TCGETS2, &tio);
    if (ret != 0) {
        LOG_CRITICAL(NULL, "failed ioctl(TCGETS2): %s", strerror(errno));
        return false;
    }
    tio.c_cflag &= ~CBAUD;
    tio.c_cflag |= BOTHER;
    tio.c_ispeed = proxy->serial_baud;
    tio.c_ospeed = proxy->serial_baud;

    ret = ioctl(proxy->serial_fd, TCSETS2, &tio);
    if (ret != 0) {
        LOG_CRITICAL(NULL, "failed ioctl(TCSETS2): %s", strerror(errno));
        return false;
    }

    return true;
}
