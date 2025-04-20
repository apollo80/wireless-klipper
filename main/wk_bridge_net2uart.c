/*
 * @file
 * @brief esp32c2 uart2net bridge for klipper
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wk_tasks.h"
#include "wk_bridge.h"
#include "wk_udp_log.h"

#include <string.h>
#include <stdio.h>
#include <sys/cdefs.h>

#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include <lwip/err.h>
#include <lwip/udp.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"



static const char *udp_TAG = "udp2uart";
static const char* prefix_name[] = {
    "unknown (0x00)"
    , "clientHello (0x01)"
    , "serverHello (0x02)"
    , "klipperData (0x03)"
    , "klipperDataConfirm (0x04)"
    , "mcuData (0x05)"
    , "mcuDataConfirm (0x06)"
    , "unknown (0x07)"
    , "unknown (0x08)"
    , "unknown (0x09)"
    , "unknown (0x0a)"
    , "unknown (0x0b)"
    , "unknown (0x0c)"
    , "unknown (0x0d)"
    , "clnPingReq (0x0e)"
    , "srvPingRep (0x0f)"
};


static void process__clientHello(struct bridge_config_t *bridge_config, struct pbuf *pbuf, const ip_addr_t *ip_addr, uint16_t port);
static void process__clientPingReq(struct bridge_config_t *bridge_config, struct pbuf *pbuf, const ip_addr_t *ip_addr, uint16_t port);
static void process__klipper_data(struct bridge_config_t *bridge_config, struct pbuf *pbuf, const ip_addr_t *ip_addr, uint16_t port);
static void process__mcu_data_confirm(struct bridge_config_t *bridge_config, struct pbuf *pbuf, const ip_addr_t *ip_addr, uint16_t port);

static void send_confirmation(struct bridge_config_t *bridge_config, uint8_t recv_index, const ip_addr_t *ip_addr, uint16_t port);

void udp_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *pbuf, const ip_addr_t *ip_addr, uint16_t port)
{
    struct bridge_config_t *bridge_config = arg;
    assert(bridge_config != NULL);
    assert(bridge_config->app_config);

    LWIP_UNUSED_ARG(pcb);
    if (pbuf == NULL) {
        return;
    }

    if (pbuf->next != NULL) {
        ESP_LOGW(udp_TAG, "recv msg: exist next pbuf");
    }

    if (pbuf->len != pbuf->tot_len) {
        ESP_LOGW(udp_TAG, "recv msg: pbuf->len != pbuf->tot_len");
    }

    struct msg_header_t *recv_header = (struct msg_header_t*) pbuf->payload;
#ifndef NDEBUG
    if (recv_header->msg_prefix > 0x0f) {
        ESP_LOGI(udp_TAG, "recv msg - incorrect prefix: %i", recv_header->msg_prefix);
        return;
    }
    ESP_LOGI(udp_TAG, "recv msg - prefix: %s; pkg_size: %u, msg_index: %u; msg_size: %u"
        , prefix_name[recv_header->msg_prefix]
        , pbuf->len
        , recv_header->msg_index
        , ntohs(recv_header->msg_size));
#endif

    // ESP_ERROR_CHECK(udp_log_init(ip_addr, 1345));

    switch (recv_header->msg_prefix) {
        case prefix_clientHello:
            process__clientHello(bridge_config, pbuf, ip_addr, port);
            break;

        case prefix_clnPingReq:
            process__clientPingReq(bridge_config, pbuf, ip_addr, port);
            break;

        case prefix_klipperData:
            process__klipper_data(bridge_config, pbuf, ip_addr, port);
            break;

        case prefix_mcuDataConfirm:
            process__mcu_data_confirm(bridge_config, pbuf, ip_addr, port);
            break;

        default:
            ESP_LOGW(udp_TAG, "incorrect msg prefix (%x) - skip it", recv_header->msg_prefix);
            break;
    }

    pbuf_free(pbuf);
}

void process__clientHello(struct bridge_config_t *bridge_config, struct pbuf *pbuf, const ip_addr_t *ip_addr, uint16_t port) {
    struct msg_header_t *recv_header = (struct msg_header_t*) pbuf->payload;
    uint16_t msg_size = ntohs(recv_header->msg_size);

    // TODO: need to add protection against resending

    bridge_config_lock();
    {
        bridge_config->msg_net_index = 0;
        bridge_config->msg_uart_index = 0;

        memcpy(&(bridge_config->source_address), ip_addr, sizeof(bridge_config->source_address));
        bridge_config->source_port = port;
    }
    bridge_config_unlock();

#ifndef NDEBUG
    uint8_t remote_address[4] = {
#if CONFIG_IDF_TARGET_ESP32C2
            (ip_addr->addr & 0x000000ff),
            (ip_addr->addr & 0x0000ff00) >> 8,
            (ip_addr->addr & 0x00ff0000) >> 16,
            (ip_addr->addr & 0xff000000) >> 24
#elif CONFIG_IDF_TARGET_ESP8266
            (ip_addr->u_addr.ip4.addr & 0x000000ff),
            (ip_addr->u_addr.ip4.addr & 0x0000ff00) >> 8,
            (ip_addr->u_addr.ip4.addr & 0x00ff0000) >> 16,
            (ip_addr->u_addr.ip4.addr & 0xff000000) >> 24
#else
#error Unknown IDF_TARGET
#endif
    };
    ESP_LOGI(udp_TAG, "start new session with %u.%u.%u.%u:%u"
        , remote_address[0], remote_address[1], remote_address[2], remote_address[3], port);
#endif

    // stop web server?
    // webctrl_stop();

    // send response
    {
        struct pbuf* send_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(struct msg_header_t) + msg_size, PBUF_RAM);
        if (send_pbuf == NULL) {
            ESP_LOGE(udp_TAG, "pbuf_alloc() failed " __FILE__ ":%i", __LINE__);
            esp_restart();
        }

        struct msg_header_t* send_header = send_pbuf->payload;

        send_header->msg_prefix = prefix_serverHello;
        send_header->msg_index  = 0;
        send_header->msg_size   = htons(msg_size);

        memcpy(send_pbuf->payload + sizeof(struct msg_header_t)
            , pbuf->payload + sizeof(struct msg_header_t)
            , msg_size);

        err_t ret = udp_sendto(bridge_config->udp_socket, send_pbuf, ip_addr, port);
        pbuf_free(send_pbuf);

        if (ret != ESP_OK) {
            ESP_LOGE(udp_TAG, "clientHello failed: response sendto(): ret %i", ret);
            return;
        }

        ESP_LOGI(udp_TAG, "send server hello - msg_index %u, msg_size %u ok", recv_header->msg_index, msg_size);
    }

    // start session timer
    xTimerStop(bridge_config->timer__session, 5);
    xTimerStart(bridge_config->timer__session, 5);
}

void process__clientPingReq(struct bridge_config_t *bridge_config, struct pbuf *pbuf, const ip_addr_t *ip_addr, uint16_t port) {
    struct msg_header_t *recv_header = (struct msg_header_t*) pbuf->payload;
    uint16_t msg_size = ntohs(recv_header->msg_size);

    // checking address and port of client
    bool is_correct_ping = true;
    bridge_config_lock();
    {
#if CONFIG_IDF_TARGET_ESP32C2
        if (bridge_config->source_address.addr != ip_addr->addr) {
            ESP_LOGE(udp_TAG, "clientPing: incorrect client address: %lx (waiting %lx)"
            , ip_addr->addr, bridge_config->source_address.addr);
            is_correct_ping &= true;
        }
#elif CONFIG_IDF_TARGET_ESP8266
         if (bridge_config->source_address.u_addr.ip4.addr != ip_addr->u_addr.ip4.addr) {
             ESP_LOGE(udp_TAG, "clientPing: incorrect client address: %x (waiting %x)"
                , ip_addr->u_addr.ip4.addr, bridge_config->source_address.u_addr.ip4.addr);
             is_correct_ping &= true;
         }
#else
#error Unknown IDF_TARGET
#endif
        if (bridge_config->source_port != port) {
            ESP_LOGE(udp_TAG, "clientPing: incorrect client port: %x (waiting %x)", port, bridge_config->source_port);
            is_correct_ping &= true;
        }
    }
    bridge_config_unlock();
    if (!is_correct_ping)
        return;

    struct pbuf* send_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(struct msg_header_t) + msg_size, PBUF_RAM);
    if (send_pbuf == NULL) {
        ESP_LOGE(udp_TAG, "pbuf_alloc() failed " __FILE__ ":%i", __LINE__);
        esp_restart();
    }

    struct msg_header_t* send_header = send_pbuf->payload;

    send_header->msg_prefix = prefix_srvPingRep;
    send_header->msg_index  = recv_header->msg_index;
    send_header->msg_size   = htons(msg_size);

    memcpy(send_pbuf->payload + sizeof(struct msg_header_t)
            , pbuf->payload + sizeof(struct msg_header_t)
            , msg_size);

    err_t ret = udp_sendto(bridge_config->udp_socket, send_pbuf, ip_addr, port);
    pbuf_free(send_pbuf);

    if (ret != ESP_OK) {
        ESP_LOGE(udp_TAG, "clientPing failed: response sendto(): ret %i", ret);
        return;
    }

    // restart session timer
    xTimerReset(bridge_config->timer__session, 5);
    ESP_LOGI(udp_TAG, "send server ping response - net_index %u; msg_size %u - ok", recv_header->msg_index, msg_size);
}

void process__klipper_data(struct bridge_config_t *bridge_config, struct pbuf *pbuf, const ip_addr_t *ip_addr, uint16_t port) {
    struct msg_header_t *recv_header = (struct msg_header_t*) pbuf->payload;
    const char* data_offset =  (const char*)(pbuf->payload + sizeof(struct msg_header_t));
    uint16_t msg_size   = ntohs(recv_header->msg_size);

    // checking address and port of client
    bool is_correct_klipper_data = true;
    bridge_config_lock();
    {
#if CONFIG_IDF_TARGET_ESP32C2
        if (bridge_config->source_address.addr != ip_addr->addr) {
            ESP_LOGE(udp_TAG, "klipper data: incorrect client address: %lx (waiting %lx)"
                    , ip_addr->addr, bridge_config->source_address.addr);
            is_correct_klipper_data &= true;
        }
#elif CONFIG_IDF_TARGET_ESP8266
        if (bridge_config->source_address.u_addr.ip4.addr != ip_addr->u_addr.ip4.addr) {
            ESP_LOGE(udp_TAG, "klipper data: incorrect client address: %x (waiting %x)"
                , ip_addr->u_addr.ip4.addr, bridge_config->source_address.u_addr.ip4.addr);
            is_correct_klipper_data &= true;
        }
#else
#error Unknown IDF_TARGET
#endif
        if (bridge_config->source_port != port) {
            ESP_LOGE(udp_TAG, "klipper data: incorrect client port: %x (waiting %x)", port, bridge_config->source_port);
            is_correct_klipper_data &= true;
        }
    }
    bridge_config_unlock();
    if (!is_correct_klipper_data)
        return;

    uint8_t stored_net_index = bridge_config->msg_net_index;
    uint16_t recv_net_index = recv_header->msg_index;
    // recv_net_index is expected to be equal to stored_net_index

    if (stored_net_index != recv_net_index) {
        if (stored_net_index == ((recv_net_index + 1) % (UINT8_MAX + 1))) {
            ESP_LOGI(udp_TAG, "recv old data msg - (net_index %u; stored net_index %u) - is duplicate; confirm it"
                 , recv_net_index, stored_net_index);

            // send confirmation
            send_confirmation(bridge_config, recv_net_index, ip_addr, port);
            return;
        }

        ESP_LOGI(udp_TAG, "recv data msg - (net_index %u; stored net_index %u) - is duplicate; skip"
            , recv_net_index, stored_net_index);
        return;
    }

    // send confirmation
    send_confirmation(bridge_config, recv_net_index, ip_addr, port);

    // and write data to uart
    write_to_uart(bridge_config, data_offset, msg_size);

    bridge_config_lock();
    bridge_config->msg_net_index++;
    bridge_config_unlock();
}

void process__mcu_data_confirm(struct bridge_config_t *bridge_config, struct pbuf *pbuf, const ip_addr_t *ip_addr, uint16_t port) {
    struct msg_header_t *recv_header = (struct msg_header_t*) pbuf->payload;

    LWIP_UNUSED_ARG(ip_addr);
    LWIP_UNUSED_ARG(port);

    bridge_config_lock();
    uint8_t stored_uart_index = bridge_config->msg_uart_index;
    SemaphoreHandle_t sem__net2uart = bridge_config->sem__net2uart;
    bridge_config_unlock();

    if (recv_header->msg_index == stored_uart_index) {
        ESP_LOGI(udp_TAG, "recv uart confirm msg - uart index: %u; stored uart index %u -> Ok"
                 , recv_header->msg_index, stored_uart_index);

        xSemaphoreGive(sem__net2uart);
    } else
        ESP_LOGW(udp_TAG, "recv confirm msg (uart_index: %u, stored uart index %u) - skip it"
                 , recv_header->msg_index, stored_uart_index);
}

void send_confirmation(struct bridge_config_t *bridge_config, uint8_t recv_index, const ip_addr_t *ip_addr, uint16_t port) {
    struct pbuf* send_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(struct msg_header_t), PBUF_RAM);
    if (send_pbuf == NULL) {
        ESP_LOGE(udp_TAG, "pbuf_alloc() failed " __FILE__ ":%i", __LINE__);
        esp_restart();
    }

    struct msg_header_t* send_header = send_pbuf->payload;

    send_header->msg_prefix = prefix_klipperDataConfirm;
    send_header->msg_index  = recv_index;
    send_header->msg_size   = 0;

    err_t ret = udp_sendto(bridge_config->udp_socket, send_pbuf, ip_addr, port);
    pbuf_free(send_pbuf);

    if (ret != ESP_OK) {
        ESP_LOGE(udp_TAG, "send confirm msg failed: response sendto(): ret %i", ret);
        return;
    }

    ESP_LOGI(udp_TAG, "send confirm msg - net_index %u; - ok", recv_index);
}

bool send_uart_data(struct bridge_config_t *bridge_config, size_t data_size) {
    static uint8_t try_count_max = 100;
    assert(data_size < bridge_config->app_config->uart_rx_buffer_size);

    uint8_t local__msg_uart_index = 0;
    SemaphoreHandle_t sem__net2uart = NULL;
    ip_addr_t send_address;
    uint16_t  send_port;
    bool session_exist = true;
    bridge_config_lock();
    {
#if CONFIG_IDF_TARGET_ESP32C2
        if (bridge_config->source_address.addr != 0) {
            memcpy(&send_address, &(bridge_config->source_address), sizeof(send_address));
        } else
            session_exist &= false;
#elif CONFIG_IDF_TARGET_ESP8266
        if (bridge_config->source_address.u_addr.ip4.addr != 0) {
            memcpy(&send_address, &(bridge_config->source_address), sizeof(send_address));
        } else
            session_exist &= false;
#else
#error Unknown IDF_TARGET
#endif

        if (bridge_config->source_port != 0) {
            send_port = bridge_config->source_port;
        } else
            session_exist &= false;

        local__msg_uart_index = bridge_config->msg_uart_index;
        sem__net2uart = bridge_config->sem__net2uart;
    }
    bridge_config_unlock();
    if (!session_exist) {
        return false;
    }

    uint8_t try_count = 0;
    while(try_count < try_count_max) {
        struct pbuf* send_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(struct msg_header_t) + data_size, PBUF_RAM);
        if (send_pbuf == NULL) {
            ESP_LOGE(udp_TAG, "pbuf_alloc() failed " __FILE__ ":%i", __LINE__);
            esp_restart();
        }

        struct msg_header_t* send_header = send_pbuf->payload;
        send_header->msg_prefix = prefix_mcuData;
        send_header->msg_index  = local__msg_uart_index;
        send_header->msg_size   = htons(data_size);

        uint8_t* data_offset = send_pbuf->payload + sizeof(struct msg_header_t);
        memcpy(data_offset, bridge_config->uart_rx_buffer, data_size);

        ESP_LOGI(udp_TAG, "send msg - uart_index: %u; msg_size: %u + %u - try_count %i ..."
            , local__msg_uart_index, sizeof(struct msg_header_t), data_size, try_count);

        err_t ret = udp_sendto(bridge_config->udp_socket, send_pbuf, &send_address, send_port);
        pbuf_free(send_pbuf);

        if (ret != ESP_OK) {
            ESP_LOGE(udp_TAG, "send msg failed - uart_index: %u; pkg_size: %u + %u - return %i"
                , local__msg_uart_index, sizeof(struct msg_header_t), data_size, ret);

            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }

        if (pdTRUE == xSemaphoreTake(sem__net2uart, 20 / portTICK_PERIOD_MS)) {
            bridge_config_lock();
            bridge_config->msg_uart_index++;
            bridge_config_unlock();
            break;
        }

        ESP_LOGI(udp_TAG, "send msg - uart_index: %u; msg_size: %u + %u, - not confirmed, try again!"
            , local__msg_uart_index, sizeof(struct msg_header_t), data_size);

        try_count++;
    }

    if (try_count < try_count_max) {

        ESP_LOGI(udp_TAG, "sended msg confirmed - uart_index: %u; msg_size: %u + %u - try_count %i"
            , local__msg_uart_index, sizeof(struct msg_header_t), data_size, try_count);

#if CONFIG_IDF_TARGET_ESP8266
        gpio_set_level(GPIO_NUM_2, 1);
#endif
        return true;
    }

    ESP_LOGE(udp_TAG, "failed send msg - uart_index: %u the number of attempts (%u) has been exhausted."
        , local__msg_uart_index, try_count_max);

    return false;
}
