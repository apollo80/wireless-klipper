/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 * @detauls settings functions
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wk_tasks.h"
#include "mdns.h"

static const char *TAG = "mdns";
static mdns_txt_item_t serviceTxtData[1];

void mdns_start()
{
    ESP_LOGI(TAG, "mDNS service starting");

    // initialize mDNS
    ESP_ERROR_CHECK( mdns_init() );

    // set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK( mdns_hostname_set(app_config()->wifi_hostname) );

    // set default mDNS instance name
    ESP_ERROR_CHECK( mdns_instance_name_set(app_config()->wifi_hostname) );

    // structure with TXT records
    serviceTxtData[0].key = "board";
    serviceTxtData[0].value = app_config()->wifi_hostname;

    //initialize service
    ESP_ERROR_CHECK( mdns_service_add("udp2uart", "_udp2uart", "_udp", app_config()->net_port, serviceTxtData, 1) );
}

void mdns_stop()
{
    mdns_free();
    ESP_LOGI(TAG, "mDNS service finished");
}