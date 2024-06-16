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
#include "driver/bridge.h"
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

void bridge_tcp2uart(void *arg) {
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
    listen_addr.sin_port = htons(app_config()->tcp_port);

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
        mdns_start();

        bridge_config->socket = accept(listen_sock, (struct sockaddr *) &source_addr, &source_addr_len);
        if (bridge_config->socket < 0) {
            ESP_LOGE(tcp_TAG, "unable to accept connection: errno %i", errno);
            return esp_restart();
        }
        ESP_LOGI(tcp_TAG, "socket accepted");
        mdns_stop();

        {
            err_code = setsockopt(bridge_config->socket, IPPROTO_TCP, TCP_NODELAY, (const void*) &yes, sizeof(int));
            if (err_code < 0) {
                ESP_LOGE(tcp_TAG, "unable to setsockopt(TCP_NODELAY): errno %i", errno);
                return esp_restart();
            }

            err_code = setsockopt(bridge_config->socket, SOL_SOCKET, SO_KEEPALIVE, (const void*) &yes, sizeof(int));
            if (err_code < 0) {
                ESP_LOGE(tcp_TAG, "unable to setsockopt(TCP_NODELAY): errno %i", errno);
                return esp_restart();
            }

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
        }

        xTaskCreate(bridge_uart2tcp, "uart2tcp_task", 2048, bridge_config, 6, &(bridge_config->task__uart2tcp));
        configASSERT(bridge_config->task__uart2tcp);

        bzero(bridge_config->tcp_rx_buffer, bridge_config->tcp_rx_buffer_size);
        int recv_bytes = 0;
        while (1) {
            union {
                struct {
                    uint32_t msg_prefix;
                    uint32_t msg_index;
                    uint32_t msg_size;
                } header;
                uint8_t raw[sizeof(uint32_t) * 3];
            } header_data;

            ESP_LOGI(tcp_TAG, "receiving data ...");
            recv_bytes = recv(bridge_config->socket, &(header_data.raw), sizeof(header_data.raw), 0);

            // Error occured during receiving
            if (recv_bytes < 0) {
                ESP_LOGE(tcp_TAG, "header recv failed: errno %i", errno);
                break;
            }

            // Connection closed
            if (recv_bytes == 0) {
                ESP_LOGI(tcp_TAG, "header: connection closed");
                break;
            }

            header_data.header.msg_prefix = ntohl(header_data.header.msg_prefix);
            header_data.header.msg_index  = ntohl(header_data.header.msg_index);
            header_data.header.msg_size   = ntohl(header_data.header.msg_size);

            if (header_data.header.msg_prefix != prefix_startMsg
                    && header_data.header.msg_prefix != prefix_tcpData
                    && header_data.header.msg_prefix != prefix_tcpConfirm) {
                ESP_LOGI(tcp_TAG, "incorrect msg prefix 0x%x", header_data.header.msg_prefix);
                break;
            }

            if (header_data.header.msg_prefix == prefix_tcpConfirm
                    || header_data.header.msg_prefix != prefix_tcpData) {
                if (header_data.header.msg_index == msg_index_get()) {
                    ESP_LOGI(tcp_TAG, "recv confirm msg (%u)", header_data.header.msg_index);

                    msg_index_update();
                    xSemaphoreGive(bridge_config->sem__tcp2uart);
                    continue;
                }
            }

            if (header_data.header.msg_size == 0) {
                ESP_LOGI(tcp_TAG, "msg data is 0 - continue next pkg");
                continue;
            }

            if (header_data.header.msg_size >= app_config()->tcp_rx_buffer_size) {
                ESP_LOGE(tcp_TAG, "msg data to big:");
                ESP_LOGE(tcp_TAG, "   msg data size - %u", header_data.header.msg_size);
                ESP_LOGE(tcp_TAG, "   buf data size - %u", app_config()->tcp_rx_buffer_size);
                continue;
            }

            // TODO: while( recv_bytes != header_data.header.msg_size ) ...
            recv_bytes = recv(bridge_config->socket, bridge_config->tcp_rx_buffer, header_data.header.msg_size, 0);
            gpio_set_level(GPIO_NUM_2, 0);

            // Error occured during receiving
            if (recv_bytes < 0) {
                ESP_LOGE(tcp_TAG, "msg data recv failed: errno %i", errno);
                break;
            }

            // Connection closed
            if (recv_bytes == 0) {
                ESP_LOGI(tcp_TAG, "msg data: connection closed");
                break;
            }

            // Data received
#ifdef NDEBUG
            int write_len = uart_write_bytes(UART_NUM_0, (const char *) bridge_config->tcp_rx_buffer, len);
            if (write_len != recv_bytes) {
                return esp_restart();;
            }
#else
            ESP_LOGI(tcp_TAG, "recv %u bytes from %u", recv_bytes, header_data.header.msg_size);
            bridge_config->tcp_rx_buffer[recv_bytes] = 0; // Null-terminate whatever we received and treat like a string
            bridge_config->tcp_rx_buffer[recv_bytes] = '\n';

            int write_len = uart_write_bytes(UART_NUM_0, (const char *) bridge_config->tcp_rx_buffer, recv_bytes + 1);
            if (write_len != (recv_bytes + 1)) {
                ESP_LOGE(tcp_TAG, "error write data to uart");
                return esp_restart();
            }
            // ESP_LOGI(tcp_TAG, "write to uart %d bytes", (write_len-1));
#endif
        }

        if (bridge_config->socket != -1) {
            vTaskDelete(bridge_config->task__uart2tcp);
            bridge_config->task__uart2tcp = NULL;

            ESP_LOGW(tcp_TAG, "Shutting down socket and restarting...");
            shutdown(bridge_config->socket, SHUT_RDWR);
            close(bridge_config->socket);
            bridge_config->socket = -1;

            gpio_set_level(GPIO_NUM_2, 1);
        }
    }

    vTaskDelete(NULL);
}
