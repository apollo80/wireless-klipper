/*
 * @file
 * @brief esp32c2 uart2net bridge for klipper
 * @details settings functions
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wk_tasks.h"
#include <sdkconfig.h>

#include <string.h>

#include <esp_log.h>
#if CONFIG_IDF_TARGET_ESP32C2
#   include <esp_mac.h>
#elif CONFIG_IDF_TARGET_ESP8266
#   include <driver/gpio.h>
#endif
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>


static const char *TAG = "wk wifi";
static wifi_config_t wifi_config;
#if CONFIG_IDF_TARGET_ESP32C2
static esp_netif_t* wifi_sta_netif = NULL;
#endif
static uint16_t s_retry_num = 0;


static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);


static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    assert(event_base == WIFI_EVENT);
//#ifndef NDEBUG
//    uint8_t mac_addr[6];
//#endif

    esp_err_t err = ESP_OK;
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "try connect to ap SSID: \'%s\' with password: \'%s\'", app_config()->wifi_ssid, app_config()->wifi_password);
            ESP_ERROR_CHECK(esp_wifi_connect());
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            err = esp_wifi_connect();
            if (err != ESP_OK) {
                ESP_LOGI(TAG, "esp_wifi_connect() return %i", err);
                return;
            }

            s_retry_num++;
//#ifndef NDEBUG
//            esp_read_mac(mac_addr, ESP_MAC_WIFI_STA);
//            ESP_LOGI(TAG, "retry to connect to the AP (%u) -> %x:%x:%x:%x:%x:%x"
//                , s_retry_num, mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
//#endif
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "connected to ap SSID: \'%s\' password: \'%s\'", app_config()->wifi_ssid, app_config()->wifi_password);
            break;

#if CONFIG_IDF_TARGET_ESP32C2
        case WIFI_EVENT_HOME_CHANNEL_CHANGE:
            ESP_LOGI(TAG, "recv event: WIFI_EVENT_HOME_CHANNEL_CHANGE");
            break;
#endif
        default:
#if CONFIG_IDF_TARGET_ESP32C2
            ESP_LOGE(TAG, "unknown wifi event %li", event_id);
#elif CONFIG_IDF_TARGET_ESP8266
            ESP_LOGE(TAG, "unknown wifi event %i", event_id);
#endif
    }
}

void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    assert(event_base == IP_EVENT);
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;

    switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "got ip:"IPSTR, IP2STR(&(event->ip_info.ip)));
            s_retry_num = 0;

#if CONFIG_IDF_TARGET_ESP8266
            wifi_blink_start(arg);
            ESP_LOGI(TAG, "blink service started");
#else
            if (! net2uart_is_started()) {
                // mdns_start();
                net2uart_start();
                uart2net_start();
                webctrl_start();
            }
#endif
            break;
        case IP_EVENT_STA_LOST_IP:
            ESP_LOGI(TAG, "lost ip:"IPSTR, IP2STR(&(event->ip_info.ip)));;
            break;

        default:
#if CONFIG_IDF_TARGET_ESP32C2
            ESP_LOGE(TAG, "unknown ip event %li", event_id);
#elif CONFIG_IDF_TARGET_ESP8266
            ESP_LOGE(TAG, "unknown ip event %i", event_id);
#endif
    }
}

void wifi_init() {
    ESP_LOGI(TAG, "wifi_init_sta starting");
    wifi_init_config_t default_cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
#if CONFIG_IDF_TARGET_ESP32C2
    wifi_sta_netif = esp_netif_create_default_wifi_sta();
#endif
    ESP_ERROR_CHECK(esp_wifi_init(&default_cfg));

    memset(&wifi_config, 0, sizeof(wifi_config));
    if (app_config()->wifi_use_sta) {
        memcpy(wifi_config.sta.ssid, app_config()->wifi_ssid, strlen(app_config()->wifi_ssid));
        memcpy(wifi_config.sta.password, app_config()->wifi_password, strlen(app_config()->wifi_password));

        /* Setting a password implies station will connect to all security modes including WEP/WPA.
         * However, these modes are deprecated and not advisable to be used. In case your Access point
         * doesn't support WPA2, these mode can be enabled by commenting below line */

        if (wifi_config.sta.password[0]) {
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        }

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    }

    ESP_ERROR_CHECK(esp_wifi_set_event_mask(WIFI_EVENT_MASK_ALL));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, app_config));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, app_config));

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    esp_log_level_set("wifi", ESP_LOG_NONE);
    ESP_ERROR_CHECK(esp_wifi_start());
}

#if CONFIG_IDF_TARGET_ESP8266
static void task__blink(void* arg) {
    ESP_LOGI(TAG, "led task started.");
    {
        gpio_config_t io_conf;

        io_conf.intr_type = GPIO_INTR_DISABLE;      // disable interrupt
        io_conf.mode = GPIO_MODE_OUTPUT;            // set as output mode
        io_conf.pin_bit_mask = 1UL << GPIO_NUM_2;   // bit mask of the pins that you want to set,e.g.GPIO15/16
        io_conf.pull_down_en = 0;                   // disable pull-down mode
        io_conf.pull_up_en = 0;                     // disable pull-up mode
        gpio_config(&io_conf);                      // configure GPIO with the given settings
    }

    for (size_t idx = 3; idx != 0; idx--) {
        gpio_set_level(GPIO_NUM_2, 0);
        ESP_LOGI(TAG, "led on.");

        vTaskDelay(150 / portTICK_PERIOD_MS);

        gpio_set_level(GPIO_NUM_2, 1);
        ESP_LOGI(TAG, "led off.");

        vTaskDelay(150 / portTICK_PERIOD_MS);
    }

    if (! net2uart_is_started()) {
        // mdns_start();
        net2uart_start();
        uart2net_start();
        webctrl_start();
    }

    vTaskDelete(NULL);
    ESP_LOGI(TAG, "led task finish.");
}

void wifi_blink_start() {
    xTaskCreate(task__blink, "wifi blink", 1024, NULL, 12, NULL);
}
#endif
