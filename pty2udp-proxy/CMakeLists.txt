cmake_minimum_required(VERSION 3.5)
project(pty2udp-proxy)

add_executable(${PROJECT_NAME}
        config.c
        log.c
        main.c

        network.c
        network_cb_recv.c
        network_client_hello.c
        network_misc.c
        network_ping.c

        pty2udp-proxy.c

        resend_cb.c

        serial.c
        serial_baud.c
        serial_cb_recv.c)

target_link_libraries(${PROJECT_NAME} PUBLIC event z)
