/*
 * @file
 * @brief esp32c2 uart2net bridge for klipper
 * @details settings functions
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wk_tasks.h"
#include "wk_settings.h"
#include "wk_udp_log.h"

void app_main() {
    app_config_read();

    uart_init();
    udp_init();
    ip_addr_t udp_log_server = {

        // server -> 192.168.127.1
#if CONFIG_IDF_TARGET_ESP32C2
        .addr = 0x017fa8c0
#elif CONFIG_IDF_TARGET_ESP8266
        .u_addr.ip4.addr = 0x017fa8c0,
        .type = IPADDR_TYPE_V4
#else
#error Unknown IDF_TARGET
#endif
    };

    udp_log_init(&udp_log_server, 1345);
    wifi_init();
}
