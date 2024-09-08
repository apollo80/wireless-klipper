/*
 * @file
 * @brief esp32c2 uart2net bridge for klipper
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

static union uni_header_t send_confirm;

static SemaphoreHandle_t sem__net2uart = NULL;

static struct sockaddr_in source_address;
static socklen_t          source_address_len;

static void send_confirmation(struct bridge_config_t *bridge_config);
static void write_to_uart(struct bridge_config_t *bridge_config);
static void process__msg_confirm(struct bridge_config_t *bridge_config, bool exist_session);
void process__msg_start(struct bridge_config_t *bridge_config, bool exist_session);
static void process__msg_data(struct bridge_config_t *bridge_config, bool exist_session);

void bridge_net2uart(void *arg) {
    ESP_LOGI(udp_TAG, "udp2uart service started");
    struct bridge_config_t *bridge_config = arg;
    assert(bridge_config);
    assert(bridge_config->app_config);

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
    size_t   net_rx_buffer_size = app_config()->net_rx_buffer_size;

    struct msg_header_t *recv_header = (struct msg_header_t*) net_rx_buffer;
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

        recv_header->msg_prefix = ntohl(recv_header->msg_prefix);
        recv_header->net_index  = ntohl(recv_header->net_index);
        recv_header->uart_index = ntohl(recv_header->uart_index);
        recv_header->msg_size   = ntohl(recv_header->msg_size);

        ESP_LOGI(udp_TAG, "recv msg - prefix: 0x%08lx; net_index: %lu; uart_index: %lu; msg_size: %lu", recv_header->msg_prefix
            , recv_header->net_index, recv_header->uart_index, recv_header->msg_size);


        bridge_config_lock();
        bool exist_session = (bridge_config->task__uart2net != NULL);
        bridge_config_unlock();

        if (exist_session)  {
            xTimerReset(bridge_config->timer__session, 5);
        }

        switch (recv_header->msg_prefix) {
            case prefix_uartConfirm:
                process__msg_confirm(bridge_config, exist_session);
                break;

            case prefix_netStart:
                process__msg_start(bridge_config, exist_session);
                break;

            case prefix_netData:
                process__msg_data(bridge_config, exist_session);
                break;

            default:
                ESP_LOGW(udp_TAG, "incorrect msg prefix (%lx) - skip it", recv_header->msg_prefix);
                break;
        }
    }

    vTaskDelete(NULL);
}

void process__msg_confirm(struct bridge_config_t *bridge_config, bool exist_session) {
    struct msg_header_t *recv_header = (struct msg_header_t*) bridge_config->net_rx_buffer;

    if (!exist_session) {
        ESP_LOGI(udp_TAG, "recv uart confirm msg - net_index %lu; uart_index: %lu - but session not started -> skip msg"
                 , recv_header->net_index, recv_header->uart_index);

        return;
    }

    bridge_config_lock();
    uint32_t stored_uart_index = bridge_config->msg_uart_index;
    bridge_config_unlock();

    if (recv_header->uart_index == stored_uart_index) {
        ESP_LOGI(udp_TAG, "recv uart confirm msg - net index %lu; uart index: %lu; stored uart index %lu -> Ok"
                 , recv_header->net_index, recv_header->uart_index, stored_uart_index);

        xSemaphoreGive(sem__net2uart);
    } else
        ESP_LOGW(udp_TAG, "recv confirm msg (net_index %lu; uart_index: %lu) - skip it"
                 , recv_header->net_index, recv_header->uart_index);
}

void process__msg_start(struct bridge_config_t *bridge_config, bool exist_session) {
    struct msg_header_t *recv_header = (struct msg_header_t*) bridge_config->net_rx_buffer;

    if(exist_session) {
        ESP_LOGI(udp_TAG, "recv start msg - duplicate");
        return;
    }

    bridge_config_lock();
    {
        bridge_config->msg_net_index = recv_header->net_index;
        bridge_config->msg_uart_index = recv_header->uart_index;

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
                 remote_address[2], remote_address[3], ntohs(source_address.sin_port));
#endif
    }
    bridge_config_unlock();

    if(bridge_config->task__uart2net) {
        vTaskDelete(bridge_config->task__uart2net);
        bridge_config->task__uart2net = NULL;

        uart_reinit();
    }

    bridge_uart2net__start(bridge_config);
    ESP_LOGI(udp_TAG, "recv start msg - uart msg index reset");

    // send confirmation
    send_confirmation(bridge_config);

    // and write data to uart
    write_to_uart(bridge_config);
}

void process__msg_data(struct bridge_config_t *bridge_config, bool exist_session)
{
    struct msg_header_t *recv_header = (struct msg_header_t*) bridge_config->net_rx_buffer;

    uint16_t stored_net_index = bridge_config->msg_net_index;
    uint16_t recv_net_index = recv_header->net_index;
    // recv_net_index is expected to be equal to stored_net_index

    // correction for intersections in the range of 2^8
    if (stored_net_index > 250 && recv_net_index < 5) {
        recv_net_index += (UINT8_MAX + 1);
    }

    if (recv_net_index <= stored_net_index) {
        ESP_LOGI(udp_TAG, "recv data msg - (net_index %lu; uart_index: %lu) - is duplicate - stored net_index %u"
                , recv_header->net_index, recv_header->uart_index, stored_net_index);
        return;
    }

    bridge_config_lock();
    bridge_config->msg_net_index = recv_header->net_index;
    bridge_config_unlock();


    bridge_config_lock();
    uint32_t local__msg_uart_index = bridge_config->msg_uart_index;
    bridge_config_unlock();

    if (recv_header->uart_index == local__msg_uart_index) {
        ESP_LOGI(udp_TAG, "recv data msg - confirm(%lu) - Ok", recv_header->uart_index);

        xSemaphoreGive(sem__net2uart);
    }

    // send confirmation
    send_confirmation(bridge_config);

    // and write data to uart
    write_to_uart(bridge_config);
}

static void send_confirmation(struct bridge_config_t *bridge_config) {
    struct msg_header_t *recv_header = (struct msg_header_t*) bridge_config->net_rx_buffer;

    send_confirm.header.msg_prefix = htonl(prefix_netConfirm);
    send_confirm.header.net_index  = htonl(recv_header->net_index);
    send_confirm.header.uart_index = htonl(recv_header->uart_index);
    send_confirm.header.msg_size   = 0;
    ssize_t sent_bytes = sendto(listen_sock, send_confirm.raw, sizeof(send_confirm), 0
                                , (struct sockaddr *) &(source_address), source_address_len);

    assert(sent_bytes == sizeof(send_confirm));
    // if (sent_bytes < 0) {
    //     ESP_LOGE(udp_TAG, "sendto() failed: errno %i", errno);
    //     break;
    // }

    ESP_LOGI(udp_TAG, "send confirm msg - net_index %lu; uart_index: %lu - ok", recv_header->net_index, recv_header->uart_index);
    udp_log(listen_sock, &source_address
            , "net2uart: send confirm msg - net_index %lu; uart_index: %lu - ok", recv_header->net_index, recv_header->uart_index);
}

void write_to_uart(struct bridge_config_t *bridge_config) {
    struct msg_header_t *recv_header = (struct msg_header_t*) bridge_config->net_rx_buffer;
    uint8_t *net_rx_buffer = bridge_config->net_rx_buffer;

    // ESP_LOGI(udp_TAG, "recv %u bytes, data %u", recv_bytes, recv_header->msg_size);
    int write_len = uart_write_bytes(UART_NUM_0
                                     , (const char *)net_rx_buffer + sizeof(struct msg_header_t), recv_header->msg_size);

    if (write_len != recv_header->msg_size) {
        ESP_LOGE(udp_TAG, "error write data to uart");
        return esp_restart();
    }

    ESP_LOGI(udp_TAG, "write to uart %d bytes", write_len);

#if CONFIG_IDF_TARGET_ESP8266
    gpio_set_level(GPIO_NUM_2, 0);
#endif
}
