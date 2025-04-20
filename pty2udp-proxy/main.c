/*
 * @file
 * @brief pty2udp proxy for klipper
 * @details
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "callbacks.h"
#include "config.h"
#include "pty2udp-proxy.h"

#include <libgen.h>
#include <event2/event.h>

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static void cb_system_signal(int signal, short events, void* args);

int main(int argc, char** argv)
{
    // check argument
    if (argc == 1) {
        fprintf(stdout,
                "Usage:\n"
                "    %s <config file>\n", basename(argv[0]));

        return EXIT_FAILURE;
    }

    // check config file exist
    if (0 != access(argv[1], F_OK)) {
        fprintf(stderr,
                "Error: failed read file %s\n"
                "Usage:\n"
                "    %s <config file>\n"
                , argv[1], basename(argv[0]));
        return EXIT_FAILURE;
    }

    // read config
    config_read(argv[1]);
    if (0 == pty2udp_proxy_count()) {
        fprintf(stderr,
                "Error config: no proxy settings\n");
        return EXIT_FAILURE;
    }

    struct event_base *default_ev_loop = event_base_new();
    if (NULL == default_ev_loop) {
        fprintf(stderr,
                "Error: failed initialize event loop library\n");
        return EXIT_FAILURE;
    }

    for(size_t idx = 0; idx < pty2udp_proxy_count(); idx++) {
        pty2udp_proxy_init(idx, default_ev_loop);
    }

    // init signal
    struct event *ev_sigTERM = evsignal_new(default_ev_loop, SIGTERM, cb_system_signal, default_ev_loop);
    evsignal_add(ev_sigTERM, NULL);

    // init signal
    struct event *ev_sigINT = evsignal_new(default_ev_loop, SIGINT, cb_system_signal, default_ev_loop);
    evsignal_add(ev_sigINT, NULL);

    // run loop
    int ret = event_base_dispatch(default_ev_loop);
    // TODO: нужно проверять код возврата (?)

    // free memory
    for(size_t idx = 0; idx < pty2udp_proxy_count(); idx++) {
        pty2udp_proxy_reset(idx);
    }

    event_free(ev_sigINT);
    event_free(ev_sigTERM);
    event_base_free(default_ev_loop);
    return EXIT_SUCCESS;
}

void cb_system_signal(int signal, short events, void* args) {
    struct event_base *base = args;

    LOG_INFO(NULL, "received signal %i", signal);
    event_base_loopbreak(base);
}