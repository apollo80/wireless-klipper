idf_component_register(
    SRCS
        app_main.c
        mdns.c
        webctrl.c
        wifi.c
        wk_bridge.c
        wk_bridge_uart2udp.c
        wk_bridge_udp2uart.c
        wk_settings.c

    INCLUDE_DIRS
        "./"

    REQUIRES
        app_update
        esp8266
        esp_http_server
        freertos
        lwip
        nvs_flash
        mdns
        pthread
        tcpip_adapter
        wpa_supplicant)
