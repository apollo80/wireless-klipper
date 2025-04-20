## ESP8266
if (${IDF_TARGET} STREQUAL "esp8266")
    set(SOURCES
        app_main.c
        #        mdns.c
        webctrl.c
        wifi.c
        wk_bridge.c
        wk_bridge_uart2net.c
        wk_bridge_net2uart.c
        wk_settings.c
        wk_udp_log.c
    )

    set(REQUIRES_COMPONENTS
        app_update
        esp8266
        esp_http_server
        freertos
        log
        lwip
        nvs_flash
        pthread
        wpa_supplicant
    )
endif()


## ESP32C2
if (${IDF_TARGET} STREQUAL "esp32c2")
    set(SOURCES
        app_main.c
        #        mdns.c
        webctrl.c
        wifi.c
        wk_bridge.c
        wk_bridge_uart2net.c
        wk_bridge_net2uart.c
        wk_settings.c
        wk_udp_log.c
    )

    set(REQUIRES_COMPONENTS
        app_update
        driver
        esp_http_server
        esp_wifi
        freertos
        log
        lwip
        nvs_flash
        pthread
        wpa_supplicant
    )
endif()

## build target
idf_component_register(
        SRCS            ${SOURCES}
        INCLUDE_DIRS    "./"
        REQUIRES        ${REQUIRES_COMPONENTS}
    )
