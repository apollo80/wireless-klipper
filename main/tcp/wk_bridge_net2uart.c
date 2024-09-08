/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 * @detauls settings functions
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wk_tasks.h"
#include "wk_bridge.h"

#include <stdio.h>
#include <sys/cdefs.h>

#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"

#include "lwip/err.h"
#include "lwip/sockets.h"

#define TCP_PORT    8888
static const char *tcp_TAG = "tcp2uart";

static int listen_sock = -1;

static struct sockaddr_in   listen_addr;
static struct sockaddr_in   source_addr;
static socklen_t            source_addr_len = sizeof(source_addr);

static int          err_code = 0;
static const int    yes = 1;

void bridge_net2uart(void *arg) {
    ESP_LOGI(tcp_TAG, "tcp2uart service started");
    struct bridge_config_t *bridge_config = arg;

    assert(bridge_config);

    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(tcp_TAG, "unable to create socket: errno %d", errno);
        return esp_restart();
    }
    ESP_LOGI(tcp_TAG, "socket created");

    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(app_config()->net_port);

    err_code = bind(listen_sock, (struct sockaddr *) &listen_addr, sizeof(listen_addr));
    if (err_code != 0) {
        ESP_LOGE(tcp_TAG, "socket unable to bind: errno %d", errno);
        return esp_restart();
    }
    ESP_LOGI(tcp_TAG, "socket binded");

    err_code = listen(listen_sock, 1);
    if (err_code != 0) {
        ESP_LOGE(tcp_TAG, "error occured during listen: errno %d", errno);
        return esp_restart();
    }
    ESP_LOGI(tcp_TAG, "socket listening");

    while (1) {
        // mdns_start();

        if (bridge_config->socket == -1) {

            bridge_config->socket = accept(listen_sock, (struct sockaddr *)&source_addr, &source_addr_len);
            if (bridge_config->socket < 0) {
                ESP_LOGE(tcp_TAG, "unable to accept connection: errno %i", errno);
                return esp_restart();
            }
            ESP_LOGI(tcp_TAG, "socket accepted");
            // mdns_stop();

            {
                err_code = setsockopt(bridge_config->socket, IPPROTO_TCP, TCP_NODELAY, (const void *)&yes, sizeof(int));
                if (err_code < 0) {
                    ESP_LOGE(tcp_TAG, "unable to setsockopt(TCP_NODELAY): errno %i", errno);
                    return esp_restart();
                }

                /*
                err_code = setsockopt(bridge_config->socket, SOL_SOCKET, SO_KEEPALIVE, (const void *)&yes, sizeof(int));
                if (err_code < 0) {
                    ESP_LOGE(tcp_TAG, "unable to setsockopt(TCP_NODELAY): errno %i", errno);
                    return esp_restart();
                }

                // err_code = setsockopt(bridge_config->socket, IPPROTO_TCP, TCP_KEEPCNT, &(bridge_config->tcp_keep_count), sizeof(int));
                err_code = setsockopt(bridge_config->socket, IPPROTO_TCP, TCP_KEEPCNT, &(bridge_config->tcp_keep_count), sizeof(int));
                if (err_code < 0) {
                    ESP_LOGE(tcp_TAG, "unable to setsockopt(TCP_KEEPCNT): errno %i", errno);
                    return esp_restart();
                }

                err_code = setsockopt(bridge_config->socket, IPPROTO_TCP, TCP_KEEPIDLE, &(bridge_config->tcp_keep_idle), sizeof(int));
                if (err_code < 0) {
                    ESP_LOGE(tcp_TAG, "unable to setsockopt(TCP_KEEPIDLE): errno %i", errno);
                    return esp_restart();
                }

                err_code = setsockopt(bridge_config->socket, IPPROTO_TCP, TCP_KEEPINTVL, &(bridge_config->tcp_keep_interval), sizeof(int));
                if (err_code < 0) {
                    ESP_LOGE(tcp_TAG, "unable to setsockopt(TCP_KEEPINTVL): errno %u", errno);
                    return esp_restart();
                }
                 */
            }

            xTaskCreate(bridge_uart2net, "uart2net_task", 2048, bridge_config, 6, &(bridge_config->task__uart2net));
            configASSERT(bridge_config->task__uart2net);

            continue;
        }


        ESP_LOGI(tcp_TAG, "receiving data ...");
        int recv_bytes = recv(bridge_config->socket, bridge_config->net_rx_buffer, bridge_config->app_config->net_rx_buffer_size, 0);

        // Error occured during receiving
        if (recv_bytes < 0) {
            ESP_LOGE(tcp_TAG, "header recv failed: errno %i", errno);
            closesocket(bridge_config->socket);
            bridge_config->socket = -1;

            vTaskDelete(bridge_config->task__uart2net);
            bridge_config->task__uart2net = NULL;
            continue;
        }

        // Connection closed
        if (recv_bytes == 0) {
            ESP_LOGI(tcp_TAG, "header: connection closed");

            closesocket(bridge_config->socket);
            bridge_config->socket = -1;

            vTaskDelete(bridge_config->task__uart2net);
            bridge_config->task__uart2net = NULL;
            continue;
        }

            // Data received
#ifndef NDEBUG
        int write_len = uart_write_bytes(UART_NUM_0,  (const char *) bridge_config->net_rx_buffer, recv_bytes);
        if (write_len != recv_bytes) {
            return esp_restart();
        }
#else
        ESP_LOGI(tcp_TAG, "recv %u bytes", recv_bytes);
        bridge_config->net_rx_buffer[recv_bytes] = 0; // Null-terminate whatever we received and treat like a string
        bridge_config->net_rx_buffer[recv_bytes] = '\n';

        int write_len = uart_write_bytes(UART_NUM_0, (const char *) bridge_config->net_rx_buffer, recv_bytes + 1);
        if (write_len != (recv_bytes + 1)) {
            ESP_LOGE(tcp_TAG, "error write data to uart");
            return esp_restart();
        }
        // ESP_LOGI(tcp_TAG, "write to uart %d bytes", (write_len-1));
#endif
    }

    vTaskDelete(NULL);
}
