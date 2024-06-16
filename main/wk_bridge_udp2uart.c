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
#include "freertos/timers.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"

#include "lwip/err.h"


static const char *udp_TAG = "udp2uart";

static struct sockaddr_in listen_addr;
static int listen_sock = -1;
static int err_code = 0;

static union uni_header_t *recv_header = NULL;
static union uni_header_t send_confirm;

static SemaphoreHandle_t sem__net2uart = NULL;

static struct sockaddr_in source_address;
static socklen_t          source_address_len;



void bridge_udp2uart(void *arg) {
    ESP_LOGI(udp_TAG, "udp2uart service started");
    struct bridge_config_t *bridge_config = arg;
    assert(bridge_config);

    sem__net2uart = bridge_config->sem__net2uart;

    listen_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(udp_TAG, "unable to create socket: errno %d", errno);
        return esp_restart();
    }
    ESP_LOGI(udp_TAG, "socket created");

    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(app_config()->net_port);

    err_code = bind(listen_sock, (struct sockaddr *) &listen_addr, sizeof(listen_addr));
    if (err_code != 0) {
        ESP_LOGE(udp_TAG, "socket unable to bind: errno %d", errno);
        return esp_restart();
    }
    ESP_LOGI(udp_TAG, "socket binded");
    bridge_config->socket = listen_sock;

    source_address_len = sizeof(source_address);
    memset(&(source_address), 0, source_address_len);

    uint8_t *net_rx_buffer = bridge_config->net_rx_buffer;
    size_t   net_rx_buffer_size = bridge_config->app_config->net_rx_buffer_size;
    while (1) {
        ESP_LOGI(udp_TAG, "waiting data ...");
        ssize_t recv_bytes = recvfrom(listen_sock, net_rx_buffer, net_rx_buffer_size, 0
            , (struct sockaddr *) &(source_address), &(source_address_len));

        if (recv_bytes == 0) {
            continue;
        }

        // Error occured during receiving
        if (recv_bytes < 0) {
            ESP_LOGE(udp_TAG, "recvfrom() failed: errno %i", errno);
            break;
        }

        recv_header = (void *)net_rx_buffer;
        recv_header->header.msg_prefix = ntohl(recv_header->header.msg_prefix);
        recv_header->header.net_index  = ntohl(recv_header->header.net_index);
        recv_header->header.uart_index = ntohl(recv_header->header.uart_index);
        recv_header->header.msg_size   = ntohl(recv_header->header.msg_size);

        ESP_LOGI(udp_TAG, "recv msg - net_index: %u; uart_index: %u; msg_size: %u"
            , recv_header->header.net_index, recv_header->header.uart_index, recv_header->header.msg_size);

        if (recv_header->header.msg_prefix == prefix_netStart
                && recv_header->header.msg_prefix == prefix_netData
                && recv_header->header.msg_prefix != prefix_uartConfirm) {
            ESP_LOGW(udp_TAG, "incorrect msg prefix (%x) - skip it", recv_header->header.msg_prefix);
            continue;
        }

        bridge_config_lock();
        bool exist_session = (bridge_config->source_address.sin_port > 0);
        bridge_config_unlock();

        if (exist_session)  {
            xTimerReset(bridge_config->timer__session, 5);
        }

        // is uart confirm message ?
        if (recv_header->header.msg_prefix == prefix_uartConfirm) {

            if (!exist_session) {
                ESP_LOGI(udp_TAG, "recv confirm msg - net_index %u; uart_index: %u - but session not started -> skip msg"
                    , recv_header->header.net_index, recv_header->header.uart_index );

                continue;
            }

            bridge_config_lock();
            uint32_t local__msg_uart_index = bridge_config->msg_uart_index;
            bridge_config_unlock();

            if (recv_header->header.uart_index == local__msg_uart_index) {
                ESP_LOGI(udp_TAG, "recv confirm msg - net_index %u; uart_index: %u -> Ok"
                    , recv_header->header.net_index, recv_header->header.uart_index);

                xSemaphoreGive(sem__net2uart);
            } else
                ESP_LOGW(udp_TAG, "recv confirm msg (net_index %u; uart_index: %u) - skip it"
                    , recv_header->header.net_index, recv_header->header.uart_index);

            continue;
        }


        //
        // (recv_header->header.msg_prefix == prefix_netData
        //  || recv_header->header.msg_prefix == prefix_netStart)
        //

        if (bridge_config->msg_net_index
            && bridge_config->msg_net_index >= recv_header->header.net_index) {
            ESP_LOGI(udp_TAG, "recv data msg - (net_index %u; uart_index: %u) - dublicate"
                , recv_header->header.net_index, recv_header->header.uart_index);
            continue;
        }

        bridge_config_lock();
        bridge_config->msg_net_index = recv_header->header.net_index;
        bridge_config_unlock();

        if (recv_header->header.msg_prefix == prefix_netStart) {

            if(exist_session) {
                ESP_LOGI(udp_TAG, "recv start msg - dublicate");
                continue;
            }

            bridge_config_lock();
            {
                memcpy(&(bridge_config->source_address), &source_address, sizeof(bridge_config->source_address));
                bridge_config->source_address_len = source_address_len;
#ifndef NDEBUG
                uint8_t remote_address[4] = {
                        (source_address.sin_addr.s_addr & 0x000000ff),
                        (source_address.sin_addr.s_addr & 0x0000ff00) >> 8,
                        (source_address.sin_addr.s_addr & 0x00ff0000) >> 16,
                        (source_address.sin_addr.s_addr & 0xff000000) >> 24
                };
                ESP_LOGI(udp_TAG, "new session - %u.%u.%u.%u:%u", remote_address[0], remote_address[1],
                         remote_address[2], remote_address[3], source_address.sin_port);
#endif
            }
            bridge_config_unlock();

            // reset UART data
            uart_flush_input(UART_NUM_0);
            uart_flush(UART_NUM_0);

            // start session timer
            xTimerStart(bridge_config->timer__session, 5);

            // create UART task
            xTaskCreate(bridge_uart2udp, "uart2udp_task", 2048, bridge_config, 6, &(bridge_config->task__uart2net));
            configASSERT(bridge_config->task__uart2tcp);

            ESP_LOGI(udp_TAG, "recv start msg - uart msg index reset");

        } else {
            bridge_config_lock();
            uint32_t local__msg_uart_index = bridge_config->msg_uart_index;
            bridge_config_unlock();

            if (recv_header->header.uart_index == local__msg_uart_index) {
                ESP_LOGI(udp_TAG, "recv data msg - confirm(%u) - Ok", recv_header->header.uart_index);

                xSemaphoreGive(sem__net2uart);
            }
        }

        // send confirmation
        {
            send_confirm.header.msg_prefix = htonl(prefix_netConfirm);
            send_confirm.header.net_index  = htonl(recv_header->header.net_index);
            send_confirm.header.uart_index = htonl(recv_header->header.uart_index);
            send_confirm.header.msg_size   = 0;
            ssize_t sent_bytes = sendto(listen_sock, send_confirm.raw, sizeof(send_confirm), 0
                , (struct sockaddr *) &(source_address), source_address_len);

            if (sent_bytes < 0) {
                ESP_LOGE(udp_TAG, "sendto() failed: errno %i", errno);
                break;
            }

            ESP_LOGI(udp_TAG, "send confirm msg - net_index %u; uart_index: %u"
                , recv_header->header.net_index, recv_header->header.uart_index);

            assert(sent_bytes == sizeof(send_confirm));
        }

        // Data received
#ifdef NDEBUG
        int write_len = uart_write_bytes(UART_NUM_0
            , (const char *) net_rx_buffer + sizeof(struct msg_header_t), recv_header->header.msg_size);

        if (write_len != recv_bytes) {
            return esp_restart();
        }
#else
        //ESP_LOGI(udp_TAG, "recv %u bytes, data %u", recv_bytes, recv_header->header.msg_size);
        net_rx_buffer[recv_bytes] = 0; // Null-terminate whatever we received and treat like a string
        net_rx_buffer[recv_bytes] = '\n';

        int write_len = uart_write_bytes(UART_NUM_0
                , (const char *)net_rx_buffer + sizeof(struct msg_header_t), recv_header->header.msg_size + 1);

        if (write_len != (recv_header->header.msg_size + 1)) {
            ESP_LOGE(udp_TAG, "error write data to uart");
            return esp_restart();
        }

        // ESP_LOGI(udp_TAG, "write to uart %d bytes", (write_len-1));
#endif
        uart_wait_tx_done(UART_NUM_0, 500 / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_NUM_2, 0);
    }

    vTaskDelete(NULL);
}
