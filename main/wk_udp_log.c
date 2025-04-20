/*
 * @file
 * @brief udp_log for klipper
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wk_udp_log.h"
#include <esp_log.h>

#include <lwip/sockets.h>
#include <lwip/udp.h>


#if CONFIG_IDF_TARGET_ESP32C2
static vprintf_like_t old_function = NULL;
#elif CONFIG_IDF_TARGET_ESP8266
static putchar_like_t old_function = NULL;
#else
#error Unknown IDF_TARGET
#endif

#ifndef UDP_LOGGING_MAX_PAYLOAD_LEN
#define UDP_LOGGING_MAX_PAYLOAD_LEN 1024
#endif

static const char *TAG = "udp_log";

static struct udp_pcb* udp_log__socket = NULL;
static ip_addr_t udp_log__to_address = {
#if CONFIG_IDF_TARGET_ESP32C2
#elif #if CONFIG_IDF_TARGET_ESP8266
    .u_addr.ip4.addr = 0
    , .type = IPADDR_TYPE_V4
#else
#error Unknown IDF_TARGET
#endif
};
static uint16_t usp_log__to_port = 0;
static SemaphoreHandle_t  sem__udp_log__buffer = NULL;

#if CONFIG_IDF_TARGET_ESP32C2
static int udp_log__vprintf( const char *str, va_list l );

#elif CONFIG_IDF_TARGET_ESP8266
static char udp_log__buffer[UDP_LOGGING_MAX_PAYLOAD_LEN];
static uint16_t udp_log__buffer_cursor = 0;
static int udp_log__putchar(int ch);

#else
#error Unknown IDF_TARGET
#endif



esp_err_t udp_log_init(const ip_addr_t* to_address, uint16_t port) {
    if (old_function) {
        return ESP_OK;
    }

    if (sem__udp_log__buffer == NULL) {
        sem__udp_log__buffer = xSemaphoreCreateBinary();
    }
    xSemaphoreTake(sem__udp_log__buffer, 100 / portTICK_PERIOD_MS);

    if (udp_log__socket == NULL) {
        udp_log__socket = udp_new();
        if (udp_log__socket == NULL) {
            ESP_LOGE(TAG, "failed create udp socket - no memory");

            udp_log_free();
            xSemaphoreGive(sem__udp_log__buffer);
            return ESP_FAIL;
        }
    }

    if (to_address != NULL) {
#if CONFIG_IDF_TARGET_ESP32C2
        udp_log__to_address.addr = to_address->addr;
#elif CONFIG_IDF_TARGET_ESP8266
        udp_log__to_address.type = to_address->type;
        udp_log__to_address.u_addr.ip4.addr = to_address->u_addr.ip4.addr;
#else
#error Unknown IDF_TARGET
#endif
    }

    if (port) {
        usp_log__to_port = port;
    }

#if CONFIG_IDF_TARGET_ESP8266
    udp_log__buffer_cursor = 0;
#endif

    if (old_function == NULL) {
#if CONFIG_IDF_TARGET_ESP32C2
        old_function = esp_log_set_vprintf(&udp_log__vprintf);
#elif CONFIG_IDF_TARGET_ESP8266
        old_function = esp_log_set_putchar(&udp_log__putchar);
#else
#error Unknown IDF_TARGET
#endif
    }

    xSemaphoreGive(sem__udp_log__buffer);
    return ESP_OK;
}

void udp_log_free(void) {
    if (sem__udp_log__buffer == NULL) {
        return;
    }
    xSemaphoreTake(sem__udp_log__buffer, 100 / portTICK_PERIOD_MS);

#if CONFIG_IDF_TARGET_ESP8266
    udp_log__buffer_cursor = 0;
#endif

    if (udp_log__socket) {
        udp_remove(udp_log__socket);
    }
    udp_log__socket = NULL;

#if CONFIG_IDF_TARGET_ESP32C2
    udp_log__to_address.addr = 0;
#elif CONFIG_IDF_TARGET_ESP8266
    udp_log__to_address.type = IPADDR_TYPE_V4;
    udp_log__to_address.u_addr.ip4.addr = 0;
#else
#error Unknown IDF_TARGET
#endif

    usp_log__to_port = 0;

#if CONFIG_IDF_TARGET_ESP32C2
    (void)esp_log_set_vprintf(old_function);
#elif CONFIG_IDF_TARGET_ESP8266
    (void*)esp_log_set_putchar(old_function);
#else
#error Unknown IDF_TARGET
#endif
    old_function = NULL;

    xSemaphoreGive(sem__udp_log__buffer);
    vSemaphoreDelete(sem__udp_log__buffer);
    sem__udp_log__buffer = NULL;
}

#if CONFIG_IDF_TARGET_ESP32C2
int udp_log__vprintf(const char *str, va_list l) {

    int data_size = vsnprintf(NULL, 0, str,l);

    struct pbuf* send_pbuf = pbuf_alloc(PBUF_TRANSPORT, data_size + 1, PBUF_RAM);
    (void)vsnprintf(send_pbuf->payload, send_pbuf->len, str,l);
    //((char*)(send_pbuf->payload))[data_size] = '\n';

    err_t ret = udp_sendto(udp_log__socket, send_pbuf, &udp_log__to_address, usp_log__to_port);
    pbuf_free(send_pbuf);

    if (ret != ESP_OK) {
        return EOF;
    }
    return data_size;
}
#elif CONFIG_IDF_TARGET_ESP8266
int udp_log__putchar(int ch) {
    if (old_function == NULL) {
        return ch;
    }

    if (sem__udp_log__buffer == NULL) {
        return old_function(ch);
    }
    xSemaphoreTake(sem__udp_log__buffer, 100 / portTICK_PERIOD_MS );

    udp_log__buffer[udp_log__buffer_cursor] = (char)(ch & 0xff);
    udp_log__buffer_cursor++;

    bool need_send = false;
    if (udp_log__buffer_cursor == UDP_LOGGING_MAX_PAYLOAD_LEN)
        need_send = true;

    if (ch == '\n')
        need_send = true;

    if (ch == '\r')
        need_send = true;

    if (udp_log__socket == NULL)
        need_send = false;

    if (udp_log__to_address.u_addr.ip4.addr == 0)
        need_send = false;

    if (usp_log__to_port == 0)
        need_send = false;

    if (need_send == true) {
        struct pbuf* send_pbuf = pbuf_alloc(PBUF_TRANSPORT, udp_log__buffer_cursor, PBUF_RAM);
        memcpy(send_pbuf->payload, udp_log__buffer, udp_log__buffer_cursor);

        err_t ret = udp_sendto(udp_log__socket, send_pbuf, &udp_log__to_address, usp_log__to_port);
        pbuf_free(send_pbuf);

        if (ret != ESP_OK) {
            return EOF;
        }
        udp_log__buffer_cursor = 0;
    }

    xSemaphoreGive(sem__udp_log__buffer);
    return ch;
}
#else
#error Unknown IDF_TARGET
#endif
