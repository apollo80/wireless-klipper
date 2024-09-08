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

#include <nvs_flash.h>

void app_main() {
    app_config_read();

    uart_init();
    wifi_init();
}
