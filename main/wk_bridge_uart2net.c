/*
 * @file
 * @brief esp32c2 uart2net bridge for klipper
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wk_bridge.h"

#include <string.h>

#include <sdkconfig.h>
#include <freertos/queue.h>

#include <driver/uart.h>
#include <driver/gpio.h>

#include <esp_log.h>
#include <esp_system.h>

#include <lwip/err.h>
#include <lwip/sockets.h>


static const char *uart_TAG = "uart2udp";
static uart_event_t event;

// static TickType_t wait_time = portMAX_DELAY;
static TickType_t wait_time = 5;

static uint8_t *uart_rx_buffer = NULL;
static uint32_t uart_rx_buffer_size = 0;
static uint8_t *uart_rx_buffer_data = NULL;
static struct msg_header_t* msg_header = NULL;
static size_t  uart_rx_data_offset = 0;

static int socketfd = -1;
static struct sockaddr_in source_address;
static socklen_t          source_address_len;

static SemaphoreHandle_t  sem__net2uart = NULL;

static bool verify_input_data(uint8_t const* data, size_t data_size);
static bool send_data(uint8_t const* data, size_t data_size, struct bridge_config_t *bridge_config);

static void stk500v2_leave();

extern QueueHandle_t uart0_queue;

void bridge_uart2net(void* arg) {
    ESP_LOGI(uart_TAG, "uart2udp service started");

    struct bridge_config_t *bridge_config = arg;

    // reset UART data
    uart_flush_input(UART_NUM_0);
    // stk500v2_leave();

    bridge_config_lock();
    {
        uart_rx_buffer      = bridge_config->uart_rx_buffer;
        uart_rx_buffer_size = bridge_config->app_config->uart_rx_buffer_size;
        uart_rx_buffer_data = uart_rx_buffer + sizeof(struct msg_header_t);
        uart_rx_data_offset = 0;

        socketfd = bridge_config->socket;
        memcpy(&source_address, &(bridge_config->source_address), sizeof(source_address));
        source_address_len = bridge_config->source_address_len;

        sem__net2uart = bridge_config->sem__net2uart;

        msg_header = (struct msg_header_t *) (uart_rx_buffer);
        msg_header->msg_prefix = ntohl(prefix_uartData);
        msg_header->uart_index = ntohl(bridge_config->msg_uart_index);
    }
    bridge_config_unlock();

    // udp_log(socketfd, &source_address,  "uart2net: started...");

    while(1) {
        // verify input data
        if (uart_rx_data_offset) {
            bool need_more = verify_input_data(uart_rx_buffer_data, uart_rx_data_offset);
            if (!need_more) {
                bool is_ok = send_data(uart_rx_buffer, uart_rx_data_offset, bridge_config);
                if (is_ok)
                    uart_rx_data_offset = 0;
            }
        }

        // Waiting for UART event.
        if (pdFALSE == xQueueReceive(uart0_queue, (void *) &event, wait_time)) {
            // ESP_LOGI(uart_TAG, "not exist data");
            // udp_log(socketfd, &source_address, "uart2net: not exist data");
            continue;
        }

        switch (event.type) {
            // Event of UART receving data
            // We'd better handler data event fast, there would be much more data events than
            // other types of events. If we take too much time on data event, the queue might be full.
            case UART_DATA: {
                // ESP_LOGI(uart_TAG, "exist data - %u", event.size);
                // udp_log(socketfd, &source_address, "uart2net: exist data - %u", event.size);
                int uart_readed_bytes = uart_read_bytes(UART_NUM_0, uart_rx_buffer_data + uart_rx_data_offset, event.size, wait_time);

                if (uart_readed_bytes < 0) {
                    ESP_LOGE(uart_TAG, "Failed uart_read_bytes(): errno %i", errno);
                    esp_restart();
                    return;
                }

                if (uart_readed_bytes == 0) {
                    break;
                }

                uart_rx_data_offset += uart_readed_bytes;
                ESP_LOGI(uart_TAG, "recv data - %u (+ %i)", uart_rx_data_offset, uart_readed_bytes);
                udp_log(socketfd, &source_address, "uart2net: recv data - %u (+ %i)", uart_rx_data_offset, uart_readed_bytes);
            }; break;

            case UART_BREAK:
                ESP_LOGI(uart_TAG, "UART break event");
                break;

                // Event of UART ring buffer full
            case UART_BUFFER_FULL:
                ESP_LOGI(uart_TAG, "RX buffer full event");
                udp_log(socketfd, &source_address, "uart2net: RX buffer full event");
                // If buffer full happened, you should consider encreasing your buffer size
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(UART_NUM_0);
                xQueueReset(uart0_queue);
                break;

            // Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGI(uart_TAG, "FIFO overflow event");
                udp_log(socketfd, &source_address, "uart2net: FIFO overflow event");
                // If fifo overflow happened, you should consider adding flow control for your application.
                // The ISR has already reset the rx FIFO,
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(UART_NUM_0);
                xQueueReset(uart0_queue);
                break;

                // Event of UART frame error
            case UART_FRAME_ERR:
                ESP_LOGI(uart_TAG, "RX frame error event");
                udp_log(socketfd, &source_address, "uart2net: RX frame error event");
                break;

            case UART_PARITY_ERR:
                ESP_LOGI(uart_TAG, "RX parity event");
                udp_log(socketfd, &source_address, "uart2net: RX parity event");
                break;

            case UART_DATA_BREAK:
                ESP_LOGI(uart_TAG, "TX data and break event");
                udp_log(socketfd, &source_address, "uart2net: TX data and break event");
                break;

            case UART_PATTERN_DET:
                ESP_LOGI(uart_TAG, "pattern detected");
                udp_log(socketfd, &source_address, "uart2net: pattern detected");
                break;
#if SOC_UART_SUPPORT_WAKEUP_INT
            case UART_WAKEUP:
                ESP_LOGI(uart_TAG, "wakeup event");
                udp_log(socketfd, &source_address, "uart2net: wakeup event");
                break;
#endif
                // Others
            default:
                ESP_LOGI(uart_TAG, "unknown uart event type: %i", event.type);
                udp_log(socketfd, &source_address, "uart2net: unknown uart event type: %i", event.type);
                break;
        }
    }
}

void bridge_uart2net__start(struct bridge_config_t *bridge_config) {
    uart_rx_buffer      = bridge_config->uart_rx_buffer;
    uart_rx_buffer_data = uart_rx_buffer + sizeof(struct msg_header_t);
    uart_rx_data_offset = 0;

    socketfd = bridge_config->socket;
    memcpy(&source_address, &(bridge_config->source_address), sizeof(source_address));
    source_address_len = bridge_config->source_address_len;

    sem__net2uart = bridge_config->sem__net2uart;

    msg_header = (struct msg_header_t *) (uart_rx_buffer);
    msg_header->msg_prefix = ntohl(prefix_uartData);
    msg_header->uart_index = ntohl(bridge_config->msg_uart_index);

    // start session timer
    xTimerStart(bridge_config->timer__session, 5);

    // create UART task
    xTaskCreate(bridge_uart2net, "uart2net_task", 2048, bridge_config, 6, &(bridge_config->task__uart2net));
    configASSERT(bridge_config->task__uart2net);
}

void bridge_uart2net__stop(struct bridge_config_t *bridge_config) {

    xTimerStop(bridge_config->timer__session, 0);
    vTaskDelete(bridge_config->task__uart2net);

    bridge_config_lock();
    {
        memset(&(bridge_config->source_address), 0, bridge_config->source_address_len);
        bridge_config->source_address_len = 0;

        bridge_config->msg_net_index = 0;
        bridge_config->msg_uart_index = 0;

        bridge_config->task__uart2net = NULL;
    }
    bridge_config_unlock();

    uart_rx_buffer = NULL;
    uart_rx_buffer_data = NULL;
    uart_rx_data_offset = 0;

    socketfd = -1;
    memset(&source_address, 0, sizeof(source_address));
    source_address_len = 0;

    sem__net2uart = NULL;

    msg_header = NULL;
}

bool verify_input_data(uint8_t const* data, size_t data_size) {
    ESP_LOGI(uart_TAG, "verify input data (%u bytes) ...", data_size);
    udp_log(socketfd, &source_address, "uart2net: verify input data (%u bytes) ...", data_size);

    size_t  msg_start = 0;
    size_t  pkg_count = 0;

    while (data_size > msg_start) {
        if (data[msg_start] == 0x7e) {
            ESP_LOGI(uart_TAG, "    found sync-byte in position %u - skip it", msg_start);
            udp_log(socketfd, &source_address, "uart2net:     found sync-byte in position %u - skip it", msg_start);
            msg_start++;
            continue;
        }

        uint8_t pkg_size = data[msg_start];
        pkg_count++;
        ESP_LOGI(uart_TAG, "     found pkg - %u; msg_start %u, pkg_size %u", pkg_count, msg_start, pkg_size);
        udp_log(socketfd, &source_address, "uart2net:     found pkg - %u; msg_start %u, pkg_size %u", pkg_count, msg_start, pkg_size);
        msg_start += pkg_size;
    }

    bool need_more = (data_size < msg_start);

    ESP_LOGI(uart_TAG, "... verified - %s", (need_more ? "need more" : "is ok"));
    udp_log(socketfd, &source_address, "uart2net: ... verified - %s", (need_more ? "need more" : "is ok"));
    return need_more;
}

bool send_data(uint8_t const* data, size_t data_size, struct bridge_config_t *bridge_config) {
    static uint8_t try_count_max = 100;

    bridge_config_lock();
    uint32_t local__net_index = bridge_config->msg_net_index;
    uint32_t local__msg_uart_index = bridge_config->msg_uart_index;
    bridge_config_unlock();

    msg_header->net_index  = ntohl(local__net_index);
    msg_header->uart_index = ntohl(local__msg_uart_index);
    msg_header->msg_size   = ntohl(data_size);

    uint8_t try_count = 0;
    while(try_count < try_count_max) {
        ESP_LOGI(uart_TAG, "send msg - net_index %lu; uart_index: %lu; msg_size: %u - try_count %i ..."
                 , local__net_index, local__msg_uart_index, data_size, try_count);

        udp_log(socketfd, &source_address, "uart2net: send msg - net_index %lu; uart_index: %lu; msg_size: %u - try_count %i ..."
                , local__net_index, local__msg_uart_index, data_size, try_count);

        ssize_t sent_bytes = sendto(socketfd
                , data, data_size + sizeof(struct msg_header_t), 0
                , (struct sockaddr *)&(source_address), source_address_len);
        assert(sent_bytes == (data_size + sizeof(struct msg_header_t)));

        if (pdTRUE == xSemaphoreTake(sem__net2uart, 50 / portTICK_PERIOD_MS)) {
            bridge_config_lock();
            bridge_config->msg_uart_index++;
            bridge_config->msg_uart_index %= (UINT8_MAX + 1);
            bridge_config_unlock();
            break;
        }

        ESP_LOGI(uart_TAG, "send msg - net_index %lu; uart_index: %lu; msg_size: %u - not confirmed, try again!"
                 , local__net_index, local__msg_uart_index, data_size);
        udp_log(socketfd, &source_address, "uart2net: send msg - net_index %lu; uart_index: %lu; msg_size: %u - not confirmed, try again!"
                , local__net_index, local__msg_uart_index, data_size);

        try_count++;
    }

    if (try_count < try_count_max) {

        ESP_LOGI(uart_TAG, "sended msg confirmed - net_index %lu; uart_index: %lu; msg_size: %u - try_count %i"
                 , local__net_index, local__msg_uart_index, data_size, try_count);
        udp_log(socketfd, &source_address, "uart2net: sended msg confirmed - net_index %lu; uart_index: %lu; msg_size: %u - try_count %i"
                , local__net_index, local__msg_uart_index, data_size, try_count);

#if CONFIG_IDF_TARGET_ESP8266
        gpio_set_level(GPIO_NUM_2, 1);
#endif
        return true;
    }

    ESP_LOGE(uart_TAG, "failed send msg - uart_index: %lu the number of attempts (%u) has been exhausted."
             , local__msg_uart_index, try_count_max);
    udp_log(socketfd, &source_address, "uart2net: failed send msg - uart_index: %lu the number of attempts (%u) has been exhausted."
            , local__msg_uart_index, try_count_max);

    return false;
}

void stk500v2_leave() {
    // stk500v2 leave programmer sequence
    static const uint8_t stk500v2_seq[] = { 0x1b, 0x01, 0x00, 0x01, 0x0e, 0x11, 0x04 };

    uint32_t orig_baudrate = 0;
    ESP_ERROR_CHECK(uart_get_baudrate(UART_NUM_0, &orig_baudrate));

    ESP_ERROR_CHECK(uart_set_baudrate(UART_NUM_0, 2400));
    (void) uart_read_bytes(UART_NUM_0, uart_rx_buffer, 1, 10);

    ESP_ERROR_CHECK(uart_set_baudrate(UART_NUM_0, 115200));
    vTaskDelay(100 / portTICK_PERIOD_MS);
    (void) uart_read_bytes(UART_NUM_0, uart_rx_buffer, uart_rx_buffer_size, 10);

    (void) uart_write_bytes(UART_NUM_0, stk500v2_seq, sizeof(stk500v2_seq));
    vTaskDelay(50 / portTICK_PERIOD_MS);
    (void) uart_read_bytes(UART_NUM_0, uart_rx_buffer, uart_rx_buffer_size, 10);

    ESP_ERROR_CHECK(uart_set_baudrate(UART_NUM_0, orig_baudrate));
}