/*
 * @file
 * @brief esp32c2 uart2net bridge for klipper
 *
 * author: apollo80
 * @email: apollo80@list.ru
 */

#include "wk_settings.h"

#include <nvs_flash.h>
#include <esp_log.h>


/// @brief default configuration
struct settings_t moduleSettings = {
    /// @brief firmware version
    .version = {
        .ver = {
            .major      = 0
            , .minor    = 0
            , .revision = 3
            , .bugfix   = 14
        }
    }
#if CONFIG_IDF_TARGET_ESP32C2
    /// @brief wifi point name
    , .wifi_hostname = "esp32c2"
#elif CONFIG_IDF_TARGET_ESP8266
    /// @brief wifi point name
    , .wifi_hostname = "esp8266"
#endif

    /// @brief wifi mode
    , .wifi_use_sta = true

    /// @brief wifi SSID
    , .wifi_ssid = "klipper-x96" //, .wifi_ssid = "mks robin wifi"

    /// @brief wifi password
    , .wifi_password = "esp8266-klipper"  // , .wifi_password = "password"


    /// @brief speed of serial port
    , .uart_baud_rate = 250000
    // , .uart_baud_rate = 74880

    /// @brief serial port buffer size for receiving data
    , .uart_rx_buffer_size = 512


    /// @brief port of tcp2serial server
    , .net_port = 8888

    /// @brief buffer size for receiving data
    , .net_rx_buffer_size = 512

    /// @brief Sign of using static network addressing
    , .use_static_ip = false

    /// @brief static network addressing module
    , .static_IPaddress = { 192, 168, 4, 4 }

    /// @brief network module when using static network addressing
    , .static_netmask = { 255, 255, 255, 0 }

    /// @brief gateway address when using static addressing
    , .static_gateway = { 192, 168, 4, 1 }
};

static const char *TAG = "setting";
static const char nvs_namespace[] = "wk_klipper";


struct settings_t* app_config() {
    return &moduleSettings;
}


void app_config_read()
{
    nvs_handle out_handle;
    esp_err_t nvs_err = ESP_OK;

    ESP_ERROR_CHECK(nvs_flash_init());
    nvs_err = nvs_open(nvs_namespace, NVS_READONLY, &out_handle);
    if (nvs_err != ESP_OK) {
        app_config_write();
        return;
    }

    union {
        version_t ver;
        uint32_t  raw;
    } storage_version;

    bool need_update_config = false;
    nvs_err = nvs_get_u32(out_handle, "version",             &(storage_version.raw));
    if (moduleSettings.version.raw != storage_version.raw) {
        need_update_config = true;

        ESP_LOGI(TAG, "firmware version: %u.%u.%u-%u"
            , moduleSettings.version.ver.major
            , moduleSettings.version.ver.minor
            , moduleSettings.version.ver.revision
            , moduleSettings.version.ver.bugfix);

        ESP_LOGI(TAG, "storage  version: %u.%u.%u-%u"
            , storage_version.ver.major
            , storage_version.ver.minor
            , storage_version.ver.revision
            , storage_version.ver.bugfix);

        // TODO: a more intelligent update is needed.
        // moduleSettings.version.raw = storage_version.raw;
    }

    size_t str_length = 0;
    str_length = sizeof(moduleSettings.wifi_hostname);
    nvs_err = nvs_get_str(out_handle, "wifi_hostname",       moduleSettings.wifi_hostname, &str_length);
    need_update_config |= (nvs_err != ESP_OK);

    uint32_t use_sta = moduleSettings.wifi_use_sta;
    nvs_err = nvs_get_u32(out_handle, "wifi_use_sta",        &(use_sta));
    need_update_config |= (nvs_err != ESP_OK);


    str_length = sizeof(moduleSettings.wifi_ssid);
    nvs_err = nvs_get_str(out_handle, "wifi_ssid",           moduleSettings.wifi_ssid,     &str_length);
    need_update_config |= (nvs_err != ESP_OK);

    str_length = sizeof(moduleSettings.wifi_password);
    nvs_err = nvs_get_str(out_handle, "wifi_password",       moduleSettings.wifi_password, &str_length);
    need_update_config |= (nvs_err != ESP_OK);


    nvs_err = nvs_get_u32(out_handle, "uart_baud_rate",      &(moduleSettings.uart_baud_rate));
    need_update_config |= (nvs_err != ESP_OK);

    nvs_err = nvs_get_u16(out_handle, "uart_rx_buffer_size", &(moduleSettings.uart_rx_buffer_size));
    need_update_config |= (nvs_err != ESP_OK);

    nvs_err = nvs_get_u16(out_handle, "udp_port",            &(moduleSettings.net_port));
    need_update_config |= (nvs_err != ESP_OK);

    nvs_err = nvs_get_u16(out_handle, "udp_rx_buffer_size",  &(moduleSettings.net_rx_buffer_size));
    need_update_config |= (nvs_err != ESP_OK);

    nvs_close(out_handle);

    if (need_update_config) {
        app_config_write();
    }

    nvs_err = ESP_OK;
}

void app_config_write()
{
    nvs_handle out_handle;
    ESP_ERROR_CHECK(nvs_open(nvs_namespace, NVS_READWRITE, &out_handle));

    esp_err_t nvs_err = ESP_OK;

    nvs_err = nvs_set_u32(out_handle, "version",             moduleSettings.version.raw);

    nvs_err = nvs_set_str(out_handle, "wifi_hostname",       moduleSettings.wifi_hostname);

    nvs_err = nvs_set_u32(out_handle, "wifi_use_sta",        moduleSettings.wifi_use_sta);
    nvs_err = nvs_set_str(out_handle, "wifi_ssid",           moduleSettings.wifi_ssid);
    nvs_err = nvs_set_str(out_handle, "wifi_password",       moduleSettings.wifi_password);

    nvs_err = nvs_set_u32(out_handle, "uart_baud_rate",      moduleSettings.uart_baud_rate);
    nvs_err = nvs_set_u16(out_handle, "uart_rx_buffer_size", moduleSettings.uart_rx_buffer_size);

    nvs_err = nvs_set_u16(out_handle, "udp_port",            moduleSettings.net_port);
    nvs_err = nvs_set_u16(out_handle, "udp_rx_buffer_size",  moduleSettings.net_rx_buffer_size);

    nvs_err = ESP_OK;
    nvs_close(out_handle);
}

