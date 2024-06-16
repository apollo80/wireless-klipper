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

#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/bridge.h"
#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_system.h"

#include "lwip/err.h"
#include "lwip/sockets.h"


static const char *uart_TAG = "uart2tcp";
static uart_event_t event;
static TickType_t wait_time = portMAX_DELAY;
static TickType_t wait_min_time = portMAX_DELAY;
static size_t uart_rx_buffer_offset = 0;
static int uart_readed_bytes;


extern QueueHandle_t uart0_queue;

void bridge_uart2tcp(void *arg) {
    ESP_LOGI(uart_TAG, "uart2tcp service started");

    struct bridge_config_t *bridge_config = arg;
    uart_rx_buffer_offset = sizeof(struct msg_header_t);
    wait_min_time =  ((3 * UART_FIFO_LEN * 1000) / (app_config()->uart_baud_rate / 8)) / portTICK_RATE_MS;

    uart_flush_input(UART_NUM_0);
    while(1) {

        // Waiting for UART event.
        if (pdFALSE == xQueueReceive(uart0_queue, (void *) &event, wait_time)) {
            // ESP_LOGI(uart_TAG, "not exist data");
            wait_time = portMAX_DELAY;

            if (uart_rx_buffer_offset <= sizeof(struct msg_header_t))
                continue;

            struct msg_header_t* header = (struct msg_header_t*)bridge_config->uart_rx_buffer;

            header->msg_prefix = ntohl(prefix_uartData);
            header->msg_index  = ntohl(msg_index_get());
            header->msg_size   = ntohl(uart_rx_buffer_offset - sizeof(struct msg_header_t));

            int try_count = 3;
            while(try_count--) {
                int err = send(bridge_config->socket, bridge_config->uart_rx_buffer, uart_rx_buffer_offset, 0);
                if (err < 0) {
                    ESP_LOGE(uart_TAG, "Error occured during tcp-sending: errno %i", errno);
                    esp_restart();
                    return;
                }

                if (pdTRUE == xSemaphoreTake(bridge_config->sem__tcp2uart, 1000 / portTICK_PERIOD_MS))
                    break;

                ESP_LOGW(uart_TAG, "confirmation has not been received - try again");
            }

            if (try_count != 0) {
                ESP_LOGI(uart_TAG, "send by socket %i bytes confirmed", uart_rx_buffer_offset);

                uart_rx_buffer_offset = sizeof(struct msg_header_t);
                bzero(bridge_config->uart_rx_buffer, bridge_config->uart_rx_buffer_size);

                gpio_set_level(GPIO_NUM_2, 1);
            } else {
                ESP_LOGE(uart_TAG, "failed: send by socket %i bytes: the number of attempts (3) has been exhausted.", uart_rx_buffer_offset);

                // closesocket(bridge_config->socket);
                // bridge_config->socket = -1;
                // break;
            }
        }
        wait_time = wait_min_time;

        switch (event.type) {
            // Event of UART receving data
            // We'd better handler data event fast, there would be much more data events than
            // other types of events. If we take too much time on data event, the queue might be full.
            case UART_DATA:
                ESP_LOGI(uart_TAG, "exist data - %u", event.size);
                uart_readed_bytes = uart_read_bytes(UART_NUM_0, bridge_config->uart_rx_buffer + uart_rx_buffer_offset, event.size, wait_time);

                if (uart_readed_bytes < 0) {
                    ESP_LOGE(uart_TAG, "Failed uart_read_bytes(): errno %i", errno);
                    esp_restart();
                    return;
                }

                if (uart_readed_bytes == 0) {
                    break;
                }

                uart_rx_buffer_offset += uart_readed_bytes;
                ESP_LOGI(uart_TAG, "recv data - %u (pkg %i)", uart_rx_buffer_offset, uart_readed_bytes);
                break;

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
