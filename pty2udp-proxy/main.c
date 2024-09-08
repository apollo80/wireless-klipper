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



// struct pty2udp_proxy proxy_list[128];

int main(int argc, char** argv)
{
    // create event loop
    struct event_base *base = event_base_new();
    if (!base) {
        fprintf(stderr, "Could not initialize libevent!\n");
        return 1;
    }

    // check argument
    if (argc == 1) {
        fprintf(stdout,
        "Usage:\n"
        "    %s <config file>\n", basename(argv[0]));

        event_base_free(base);
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
        fprintf(stderr, "Error config: no proxy settings\n");
        return EXIT_FAILURE;
    }
    for(size_t idx = 0; idx < pty2udp_proxy_count(); idx++) {
        pty2udp_proxy_init(idx, base);
    }

    // init signal
    struct event *ev_sigTERM = evsignal_new(base, SIGTERM, cb_proxy__sigTERM, base);
    evsignal_add(ev_sigTERM, NULL);

    // run loop
    event_base_dispatch(base);

    // free memory
    for(size_t idx = 0; idx < pty2udp_proxy_count(); idx++) {
        pty2udp_proxy_reset(idx);
    }

    event_free(ev_sigTERM);
    event_base_free(base);

    return EXIT_SUCCESS;
}

