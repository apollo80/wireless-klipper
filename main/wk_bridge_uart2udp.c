/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 * @detauls settings functions
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wk_bridge.h"

#include <string.h>

#include <freertos/queue.h>

#include <driver/uart.h>
#include <driver/gpio.h>

#include <esp_log.h>
#include <esp_system.h>

#include <lwip/err.h>
#include <lwip/sockets.h>


static const char *uart_TAG = "uart2udp";
static uart_event_t event;

static TickType_t wait_time = portMAX_DELAY;
static TickType_t wait_min_time = portMAX_DELAY;

static uint8_t *uart_rx_buffer = NULL;
static uint8_t *uart_rx_buffer_data = NULL;
static struct msg_header_t* msg_header = NULL;
static size_t  uart_rx_data_offset = 0;

static int socketfd = -1;
static struct sockaddr_in source_address;
static socklen_t          source_address_len;

static SemaphoreHandle_t  sem__net2uart = NULL;

static int try_count = 0;
static int try_count_max = 100;


extern QueueHandle_t uart0_queue;

void bridge_uart2udp(void* arg) {
    ESP_LOGI(uart_TAG, "uart2udp service started");

    struct bridge_config_t *bridge_config = arg;

    bridge_config_lock();
    {
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

        wait_min_time =
                ((3 * UART_FIFO_LEN * 1000) / (bridge_config->app_config->uart_baud_rate / 8)) / portTICK_RATE_MS;
    }
    bridge_config_unlock();

    while(1) {

        // Waiting for UART event.
        if (pdFALSE == xQueueReceive(uart0_queue, (void *) &event, wait_time)) {
            // ESP_LOGI(uart_TAG, "not exist data");
            wait_time = portMAX_DELAY;

            if (uart_rx_data_offset == 0) {
                continue;
            }

            bridge_config_lock();
            uint32_t local__net_index = bridge_config->msg_net_index;
            uint32_t local__msg_uart_index = bridge_config->msg_uart_index;
            bridge_config_unlock();

            msg_header->net_index  = ntohl(local__net_index);
            msg_header->uart_index = ntohl(local__msg_uart_index);
            msg_header->msg_size   = ntohl(uart_rx_data_offset);

            while(try_count < try_count_max) {
                ESP_LOGI(uart_TAG, "send msg - net_index %u; uart_index: %u; msg_size: %u - try_count %i ..."
                    , local__net_index, local__msg_uart_index, uart_rx_data_offset, try_count);

                ssize_t sent_bytes = sendto(socketfd, uart_rx_buffer, uart_rx_data_offset + sizeof(struct msg_header_t), 0
                    , (struct sockaddr *)&(source_address), source_address_len);
                assert(sent_bytes == (uart_rx_data_offset + sizeof(struct msg_header_t)));

                if (pdTRUE == xSemaphoreTake(sem__net2uart, 200 / portTICK_PERIOD_MS)) {
                    bridge_config_lock();
                    bridge_config->msg_uart_index++;
                    bridge_config_unlock();
                    break;
                }

                ESP_LOGI(uart_TAG, "send msg - net_index %u; uart_index: %u; msg_size: %u - not confirmed, try again!"
                    , local__net_index, local__msg_uart_index, uart_rx_data_offset);
                try_count++;
            }

            if (try_count < try_count_max) {

                ESP_LOGI(uart_TAG, "sended msg confirmed - net_index %u; uart_index: %u; msg_size: %u - try_count %i"
                    , local__net_index, local__msg_uart_index, uart_rx_data_offset, try_count);

                uart_rx_data_offset = 0;
                try_count = 0;

                gpio_set_level(GPIO_NUM_2, 1);
            } else {
                ESP_LOGE(uart_TAG, "failed send msg - uart_index: %u the number of attempts (%u) has been exhausted."
                    , local__msg_uart_index, try_count_max);

                uart_rx_data_offset = 0;
                try_count = 0;
            }
            continue;
        }

        wait_time = wait_min_time;

        switch (event.type) {
            // Event of UART receving data
            // We'd better handler data event fast, there would be much more data events than
            // other types of events. If we take too much time on data event, the queue might be full.
            case UART_DATA: {
                // ESP_LOGI(uart_TAG, "exist data - %u", event.size);
                int uart_readed_bytes = uart_read_bytes(UART_NUM_0
                    , uart_rx_buffer_data + uart_rx_data_offset, event.size, wait_time);

                if (uart_readed_bytes < 0) {
                    ESP_LOGE(uart_TAG, "Failed uart_read_bytes(): errno %i", errno);
                    esp_restart();
                    return;
                }

                if (uart_readed_bytes == 0) {
                    break;
                }

                uart_rx_data_offset += uart_readed_bytes;
                // ESP_LOGI(uart_TAG, "recv data - %u (pkg %i)", uart_rx_data_offset, uart_readed_bytes);
            }; break;

                // Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGI(uart_TAG, "hw fifo overflow");
                // If fifo overflow happened, you should consider adding flow control for your application.
                // The ISR has already reset the rx FIFO,
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(UART_NUM_0);
                xQueueReset(uart0_queue);
                break;

                // Event of UART ring buffer full
            case UART_BUFFER_FULL:
                ESP_LOGI(uart_TAG, "ring buffer full");
                // If buffer full happened, you should consider encreasing your buffer size
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(UART_NUM_0);
                xQueueReset(uart0_queue);
                break;

            case UART_PARITY_ERR:
                ESP_LOGI(uart_TAG, "uart parity error");
                break;

                // Event of UART frame error
            case UART_FRAME_ERR:
                ESP_LOGI(uart_TAG, "uart frame error");
                break;

                // Others
            default:
                ESP_LOGI(uart_TAG, "unknown uart event type: %i", event.type);
                break;
        }
    }
}
