idf_component_register(
    SRCS
        app_main.c
#        mdns.c
        webctrl.c
        wifi.c
        wk_bridge.c
        wk_bridge_uart2net.c
        wk_bridge_net2uart.c
        wk_settings.c

    INCLUDE_DIRS
        "./"

## ESP32C2
    REQUIRES
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

## ESP8266
#    REQUIRES
#        app_update
#        esp8266
#        esp_http_server
#        freertos
#        log
#        lwip
#        nvs_flash
#        pthread
#        wpa_supplicant

#LDFRAGMENTS
    )
