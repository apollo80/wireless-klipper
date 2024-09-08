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

void cb_proxy__sigTERM(int signal, short events, void* args) {
    struct event_base *base = args;

    LOG_INFO(NULL, "received signal %i", signal);
    event_base_loopbreak(base);
}