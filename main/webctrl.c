/*
 * @file
 * @brief web control server
 * @detauls 
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wk_tasks.h"
#include "wk_settings.h"

#include <esp_http_server.h>
#include <esp_ota_ops.h>
#include <esp_log.h>
#include <esp_system.h>

#include <limits.h>
#include <inttypes.h>


#include "html.c"

#define OTA_BUF_SIZE        4096
#define CONFIG_BUF_SIZE     1024


static const char *TAG = "webctrl";
static httpd_config_t config = HTTPD_DEFAULT_CONFIG();
static httpd_handle_t server = NULL;

static esp_err_t webctrl_handler__root(httpd_req_t *http_req);
static esp_err_t webctrl_handler__update_self(httpd_req_t *http_req);
static esp_err_t webctrl_handler__get_config(httpd_req_t *http_req);
static esp_err_t webctrl_handler__set_config(httpd_req_t *http_req);
static esp_err_t webctrl_handler__restart(httpd_req_t *http_req);

static httpd_uri_t root__get = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = webctrl_handler__root,
    .user_ctx = NULL
};

static httpd_uri_t update_self__post = {
    .uri      = "/update_self",
    .method   = HTTP_POST,
    .handler  = webctrl_handler__update_self,
    .user_ctx = NULL
};

static httpd_uri_t get_config__post = {
    .uri      = "/get_config",
    .method   = HTTP_GET,
    .handler  = webctrl_handler__get_config,
    .user_ctx = NULL
};

static httpd_uri_t set_config__post = {
    .uri      = "/set_config",
    .method   = HTTP_POST,
    .handler  = webctrl_handler__set_config,
    .user_ctx = NULL
};

static httpd_uri_t restart__post = {
    .uri      = "/restart",
    .method   = HTTP_POST,
    .handler  = webctrl_handler__restart,
    .user_ctx = NULL
};

void webctrl_start()
{
    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);

    esp_err_t ret = httpd_start(&server, &config);
    if (ret == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");

        httpd_register_uri_handler(server, &root__get);
        httpd_register_uri_handler(server, &update_self__post);
        httpd_register_uri_handler(server, &get_config__post);
        httpd_register_uri_handler(server, &set_config__post);
        httpd_register_uri_handler(server, &restart__post);
        return;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return;
}

void webctrl_stop()
{
    httpd_stop(server);
    server = NULL;
}

esp_err_t webctrl_handler__root(httpd_req_t *http_req)
{
    ESP_LOGI(TAG, "webctrl_handler__root() begin.");

    ESP_ERROR_CHECK(httpd_resp_set_status(http_req, HTTPD_200));
    ESP_ERROR_CHECK(httpd_resp_set_type(http_req, HTTPD_TYPE_TEXT));
    ESP_ERROR_CHECK(httpd_resp_send(http_req, html_BODY, sizeof(html_BODY) - 1));

    ESP_LOGI(TAG, "webctrl_handler__root() end.");
    return ESP_OK;
}

esp_err_t webctrl_handler__update_self(httpd_req_t *http_req)
{
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(TAG, "Starting OTA...");
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Passive OTA partition not found");

        // TODO: send an extended error
        ESP_ERROR_CHECK(httpd_resp_set_status(http_req, HTTPD_500));
        ESP_ERROR_CHECK(httpd_resp_send(http_req, NULL, 0));
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Writing to partition subtype %u at offset 0x%lx", update_partition->subtype, update_partition->address);

    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed, error=%d", err);

        // TODO: send an extended error
        ESP_ERROR_CHECK(httpd_resp_set_status(http_req, HTTPD_500));
        ESP_ERROR_CHECK(httpd_resp_send(http_req, NULL, 0));
        return err;
    }
    ESP_LOGI(TAG, "esp_ota_begin succeeded");
    ESP_LOGI(TAG, "Please Wait. This may take time");

    esp_err_t ota_write_err = ESP_OK;
    char *upgrade_data_buf = (char *)malloc(OTA_BUF_SIZE);
    if (!upgrade_data_buf) {
        ESP_LOGE(TAG, "Couldn't allocate memory to upgrade data buffer");

        // TODO: send an extended error
        ESP_ERROR_CHECK(httpd_resp_set_status(http_req, HTTPD_500));
        ESP_ERROR_CHECK(httpd_resp_send(http_req, NULL, 0));
        return ESP_ERR_NO_MEM;
    }

    int binary_file_len = 0;
    while (1) {
        int data_read = httpd_req_recv(http_req, upgrade_data_buf, OTA_BUF_SIZE);
        if (data_read == 0) {
            ESP_LOGI(TAG, "Connection closed, all data received");
            break;
        }

        if (data_read < 0) {
            ESP_LOGE(TAG, "Error: data read error");
            break;
        }

        if (data_read > 0) {
            ota_write_err = esp_ota_write( update_handle, (const void *)upgrade_data_buf, data_read);
            if (ota_write_err != ESP_OK) {
                break;
            }
            binary_file_len += data_read;
            ESP_LOGI(TAG, "Written image length %d", binary_file_len);
        }
    }
    free(upgrade_data_buf);
    ESP_LOGI(TAG, "Total binary data length writen: %d", binary_file_len);

    esp_err_t ota_end_err = esp_ota_end(update_handle);
    if (ota_write_err != ESP_OK) {
        ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%x", ota_write_err);

        // TODO: send an extended error
        ESP_ERROR_CHECK(httpd_resp_set_status(http_req, HTTPD_500));
        ESP_ERROR_CHECK(httpd_resp_send(http_req, NULL, 0));
        return ota_write_err;
    } else if (ota_end_err != ESP_OK) {
        ESP_LOGE(TAG, "Error: esp_ota_end failed! err=0x%x. Image is invalid", ota_end_err);

        // TODO: send an extended error
        ESP_ERROR_CHECK(httpd_resp_set_status(http_req, HTTPD_500));
        ESP_ERROR_CHECK(httpd_resp_send(http_req, NULL, 0));
        return ota_end_err;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%d", err);

        // TODO: send an extended error
        ESP_ERROR_CHECK(httpd_resp_set_status(http_req, HTTPD_500));
        ESP_ERROR_CHECK(httpd_resp_send(http_req, NULL, 0));
        return err;
    }
    ESP_LOGI(TAG, "esp_ota_set_boot_partition succeeded");
    
    ESP_ERROR_CHECK(httpd_resp_set_status(http_req, HTTPD_200));
    ESP_ERROR_CHECK(httpd_resp_send(http_req, NULL, 0));
    return ESP_OK;
}

esp_err_t webctrl_handler__get_config(httpd_req_t *http_req)
{
    ESP_LOGI(TAG, "Starting config uploading...");

    char *config_buf = (char *)malloc(CONFIG_BUF_SIZE);
    if (!config_buf) {
        ESP_LOGE(TAG, "Couldn't allocate memory to upgrade data buffer");

        // TODO: send an extended error
        ESP_ERROR_CHECK(httpd_resp_set_status(http_req, HTTPD_500));
        ESP_ERROR_CHECK(httpd_resp_send(http_req, NULL, 0));
        return ESP_ERR_NO_MEM;
    }

    static const char json_template[] = 
        "{"
        "\"version\": \"%i.%i.%i-%i\","
        "\"wifi_hostname\":\"%s\","
        "\"wifi_ssid\":\"%s\","
        "\"wifi_password\":\"%s\","
        "\"use_static_ip\":%s,"
        "\"static_IPaddress\":\"192.168.4.1\","
        "\"static_netmask\":\"192.168.4.255\","
        "\"static_gateway\":\"192.168.4.1\","
        "\"uart_baud_rate_option\":[9600, 14400, 19200, 28800, 38400, 38400, 57600, 74880, 115200, 230400, 250000, 256000, 460800, 576000, 921600],"
        "\"uart_baud_rate\":%lu,"
        "\"uart_rx_buffer_size\":%u,"
        "\"tcp_port\":%u,"
        "\"tcp_rx_buffer_size\":%u"
        "}";
    struct settings_t* config = app_config();
    
    int config_size = snprintf(config_buf, CONFIG_BUF_SIZE, json_template,
        config->version.ver.major, config->version.ver.minor, config->version.ver.revision, config->version.ver.bugfix
        , config->wifi_hostname
        , config->wifi_ssid
        , config->wifi_password
        , (config->wifi_use_sta ? "true" : "false")
        , config->uart_baud_rate
        , config->uart_rx_buffer_size
        , config->net_port
        , config->net_rx_buffer_size
        );
    config_buf[config_size] = 0;

    ESP_LOGI(TAG, "config(%u): %s", config_size, config_buf);

    ESP_ERROR_CHECK(httpd_resp_set_status(http_req, HTTPD_200));
    ESP_ERROR_CHECK(httpd_resp_set_type(http_req, HTTPD_TYPE_JSON));
    ESP_ERROR_CHECK(httpd_resp_send(http_req, config_buf, config_size));

    free(config_buf);
    return ESP_OK;
}

esp_err_t webctrl_handler__set_config(httpd_req_t *http_req)
{
    ESP_LOGI(TAG, "Starting config update...");

    char *config_buf = (char *)malloc(CONFIG_BUF_SIZE);
    if (!config_buf) {
        ESP_LOGE(TAG, "Couldn't allocate memory to upgrade data buffer");

        // TODO: send an extended error
        ESP_ERROR_CHECK(httpd_resp_set_status(http_req, HTTPD_500));
        ESP_ERROR_CHECK(httpd_resp_send(http_req, NULL, 0));
        return ESP_ERR_NO_MEM;
    }

    int16_t config_size = 0;
    while (1) {
        int data_read = httpd_req_recv(http_req, config_buf + config_size, OTA_BUF_SIZE - config_size);
        if (data_read == 0) {
            ESP_LOGI(TAG, "Connection closed, all data received");
            break;
        }

        if (data_read < 0) {
            ESP_LOGE(TAG, "Error: data read error");
            break;
        }

        if (data_read > 0) {
            config_size += data_read;
            ESP_LOGI(TAG, "Written image length %d", config_size);
        }
    }
    config_buf[config_size] = 0;

    ESP_LOGD(TAG, "Total binary data length reveived: %d", config_size);
    ESP_LOGD(TAG, "Receive data: %s", config_buf);

    char*   key = NULL;
    int16_t key_size = 0;
    char*   value = NULL;
    int16_t value_size = 0;

    for(int16_t idx = 0; idx < config_size; idx++) {
        if (key == NULL) {
            if (config_buf[idx] != '=') {
                key_size++;
            }
            else {
                key = config_buf + idx - key_size;
                config_buf[idx] = 0;
            }
            continue;
        }
        
        if (value == NULL) {
            if (config_buf[idx] != '&') {
                value_size++;
                continue;
            }
            else {
                value = config_buf + idx - value_size;
                config_buf[idx] = 0;
            }
        }

#define min(a,b)    ((a) > (b) ? (b):(a))
        ESP_LOGD(TAG, "Receive parameter: %s = %s", key, value);
        
        if (0 == strncmp(key, "wifi_hostname", key_size)) {
            strncpy(app_config()->wifi_hostname, value, min(sizeof(app_config()->wifi_hostname), value_size + 1));
            ESP_LOGD(TAG, "    set 'wifi_hostname' in '%s'", app_config()->wifi_hostname);

        } else if (0 == strncmp(key, "wifi_ssid", key_size)) {
            strncpy(app_config()->wifi_ssid, value, min(sizeof(app_config()->wifi_ssid), value_size + 1));
            ESP_LOGD(TAG, "    set 'wifi_ssid' in '%s'", app_config()->wifi_ssid);

        } else if (0 == strncmp(key, "wifi_use_sta", key_size)) {
            if (0 == strncmp(value, "true", value_size)) {
                app_config()->wifi_use_sta = true;
            } else if (0 == strncmp(value, "false", value_size)) {
                app_config()->wifi_use_sta = false;
            }
            ESP_LOGD(TAG, "    set 'wifi_use_sta' in '%s'", (app_config()->wifi_use_sta ? "true" : "false"));

        } else if (0 == strncmp(key, "wifi_password", key_size)) {
            strncpy(app_config()->wifi_password, value, min(sizeof(app_config()->wifi_password), value_size + 1));
            ESP_LOGD(TAG, "    set 'wifi_password' in '%s'", app_config()->wifi_password);
/*
        } else if (0 == strncmp(key, "use_static_ip", key_size)) {
            if (0 == strncmp(value, "true", value_size)) {
                app_config()->use_static_ip = true;
            } else if (0 == strncmp(value, "false", value_size)) {
                app_config()->use_static_ip = false;
            }
            ESP_LOGD(TAG, "    set 'use_static_ip' in '%s'", (app_config()->use_static_ip ? "true" : "false"));

        } else if (0 == strncmp(key, "static_IPaddress", key_size)) {

        } else if (0 == strncmp(key, "static_IPaddress", key_size)) {

        } else if (0 == strncmp(key, "static_IPaddress", key_size)) {
*/
        } else if (0 == strncmp(key, "uart_baud_rate", key_size)) {
            char *end_ptr = NULL;
            unsigned long ret = strtoul(value, &end_ptr, 10);
            if (ret != ULONG_MAX) {
                app_config()->uart_baud_rate = ret;
            }
            ESP_LOGD(TAG, "    set 'uart_baud_rate' in '%lu'", app_config()->uart_baud_rate);

        } else if (0 == strncmp(key, "uart_rx_buffer_size", key_size)) {
            char *end_ptr = NULL;
            unsigned long ret = strtoul(value, &end_ptr, 10);
            if (ret != ULONG_MAX) {
                app_config()->uart_rx_buffer_size = ret;
            }
            ESP_LOGD(TAG, "    set 'uart_rx_buffer_size' in '%i'", app_config()->uart_rx_buffer_size);
        
        } else if (0 == strncmp(key, "tcp_port", key_size)) {
            char *end_ptr = NULL;
            unsigned long ret = strtoul(value, &end_ptr, 10);
            if (ret != ULONG_MAX) {
                app_config()->net_port = ret;
            }
            ESP_LOGD(TAG, "    set 'tcp_port' in '%i'", app_config()->net_port);
        
        } else if (0 == strncmp(key, "tcp_rx_buffer_size", key_size)) {
            char *end_ptr = NULL;
            unsigned long ret = strtoul(value, &end_ptr, 10);
            if (ret != ULONG_MAX) {
                app_config()->net_rx_buffer_size = ret;
            }
            ESP_LOGD(TAG, "    set 'tcp_rx_buffer_size' in '%i'", app_config()->net_rx_buffer_size);
        }
#undef min

        key = NULL;
        key_size = 0;
        value = NULL;
        value_size = 0;
    }

    free(config_buf);

    app_config_write();

    ESP_ERROR_CHECK(httpd_resp_set_status(http_req, HTTPD_200));
    ESP_ERROR_CHECK(httpd_resp_send(http_req, NULL, 0));
    return ESP_OK;
}

esp_err_t webctrl_handler__restart(httpd_req_t *http_req)
{
    ESP_ERROR_CHECK(httpd_resp_set_status(http_req, HTTPD_200));
    ESP_ERROR_CHECK(httpd_resp_send(http_req, NULL, 0));

    // FIXME: create task
    vTaskDelay(500 / portTICK_PERIOD_MS);
    esp_restart();
    return ESP_OK;
}
