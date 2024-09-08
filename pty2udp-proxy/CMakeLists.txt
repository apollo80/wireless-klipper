cmake_minimum_required(VERSION 3.5)
project(pty2udp-proxy)

add_executable(${PROJECT_NAME}
        callbacks_misc.c
        callbacks_network.c
        callbacks_serial.c
        callbacks_signal.c
        config.c
        log.c
        main.c
        pty2udp-proxy-network.c
        pty2udp-proxy-serial.c
        pty2udp-proxy-serial_baud.c
        pty2udp-proxy.c)

target_link_libraries(${PROJECT_NAME} PUBLIC event)
