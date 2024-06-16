/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 * @detauls settings functions
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wk_tasks.h"
#include "wk_settings.h"

#include <nvs_flash.h>

void app_main() {
    app_config_read();

    tcp2uart_init();
    wifi_init();
}
