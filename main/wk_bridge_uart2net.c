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
static size_t uart_rx_buffer_size = 0;
// static size_t send_size = 0;

// static uint8_t *uart_rx_buffer_data = NULL;

static size_t  uart_rx_data_offset = 0;

// static int socketfd = -1;
// static struct sockaddr_in source_address;
// static socklen_t          source_address_len;

SemaphoreHandle_t  sem__net2uart = NULL;

static size_t verify_input_data(uint8_t const* data, size_t data_size);
// static bool send_data(uint8_t* const data, size_t data_size, struct bridge_config_t *bridge_config);


static void stk500v2_leave();

extern QueueHandle_t uart0_queue;

void task_uart2net(void* arg) {
    ESP_LOGI(uart_TAG, "uart2udp service started");

    struct bridge_config_t *bridge_config = arg;

    // reset UART data
    uart_flush_input(UART_NUM_0);
    // stk500v2_leave();

    bridge_config_lock();
    {
        uart_rx_buffer      = bridge_config->uart_rx_buffer;
        uart_rx_buffer_size = bridge_config->app_config->uart_rx_buffer_size;
        sem__net2uart       = bridge_config->sem__net2uart;
    }
    bridge_config_unlock();

    uart_rx_data_offset = 0;
    while(1) {
        if (uart_rx_data_offset) {
            uint16_t send_size = verify_input_data(uart_rx_buffer, uart_rx_data_offset);

            if (send_size) {
                bool is_ok = send_uart_data(bridge_config, send_size);
                if (is_ok == false) {
                    ESP_LOGE(uart_TAG, "UART: connection not exist!!!");

                    uart_rx_data_offset = 0;
                    continue;
                }

                size_t before_bytes = uart_rx_data_offset;
                uart_rx_data_offset -= send_size;
                if (uart_rx_data_offset) {
                    ESP_LOGI(uart_TAG, "before %i bytes, sent %i bytes, shift %i data", before_bytes, send_size,
                             uart_rx_data_offset);
                    memmove(uart_rx_buffer, uart_rx_buffer + send_size, uart_rx_data_offset);
                }
            }
        }

        // Waiting to flush all tx data
        ESP_ERROR_CHECK(uart_wait_tx_done(UART_NUM_0, 40));

        // Waiting for UART event.
        if (pdFALSE == xQueueReceive(uart0_queue, (void *) &event, wait_time)) {
            // ESP_LOGI(uart_TAG, "not exist data");
            continue;
        }

        switch (event.type) {
            // Event of UART receiving data
            // We'd better handler data event fast, there would be much more data events than
            // other types of events. If we take too much time on data event, the queue might be full.
            case UART_DATA: {
                // ESP_LOGI(uart_TAG, "exist data - %u", event.size);
                int uart_readed_bytes = uart_read_bytes(UART_NUM_0, uart_rx_buffer + uart_rx_data_offset, event.size, wait_time);
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

#if CONFIG_IDF_TARGET_ESP32C2
            case UART_BREAK:
                ESP_LOGI(uart_TAG, "UART break event");
                uart_flush_input(UART_NUM_0);
                xQueueReset(uart0_queue);
                break;

            case UART_DATA_BREAK:
                ESP_LOGI(uart_TAG, "TX data and break event");
                udp_log(socketfd, &source_address, "uart2net: TX data and break event");
                break;

            case UART_PATTERN_DET:
                ESP_LOGI(uart_TAG, "pattern detected");
                udp_log(socketfd, &source_address, "uart2net: pattern detected");
                break;
#endif

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

size_t verify_input_data(uint8_t const* data, size_t data_size) {
    // ESP_LOGI(uart_TAG, "verify input data (%u bytes) ...", data_size);

    size_t  tmp_size = 0;
    uint8_t last_pkg_size = 0;
    // size_t  pkg_count = 0;

    while (data_size > tmp_size) {
        if (data[tmp_size] == 0x7e) {
            // ESP_LOGI(uart_TAG, "    found sync-byte in position %u - skip it", msg_start);
            tmp_size++;
            continue;
        }

        last_pkg_size = data[tmp_size];
        if (data_size < (tmp_size + last_pkg_size)) {
            if (tmp_size && data[tmp_size - 1] == 0x7e)
                tmp_size--;

            return tmp_size;
        }

        // pkg_count++;
        // ESP_LOGI(uart_TAG, "     found pkg - %u; msg_start %u, pkg_size %u", pkg_count, msg_start, pkg_size);
        tmp_size += last_pkg_size;
    }

    // ESP_LOGI(uart_TAG, "... verified - %s", (need_more ? "need more" : "is ok"));
    return tmp_size;
}


void write_to_uart(struct bridge_config_t *bridge_config, const char* buffer, size_t buf_size) {
    ESP_ERROR_CHECK(uart_wait_tx_done(UART_NUM_0, portMAX_DELAY ));

    // ESP_LOGI(udp_TAG, "recv %u bytes, data %u", recv_bytes, msg_size);
    int write_len = uart_write_bytes(UART_NUM_0, buffer, buf_size);
    if (write_len != buf_size) {
        ESP_ERROR_CHECK(uart_wait_tx_done(UART_NUM_0, portMAX_DELAY));
        ESP_LOGE(uart_TAG, "write to uart failed: error write data to uart");
        return esp_restart();
    }

    ESP_ERROR_CHECK(uart_wait_tx_done(UART_NUM_0, portMAX_DELAY));
    if (write_len > 4)
        ESP_LOGI(uart_TAG, "write to uart %d bytes: %02x %02x %02x %02x %02x ...", write_len
            , buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
    else
        ESP_LOGI(uart_TAG, "write to uart %d bytes", write_len);

#if CONFIG_IDF_TARGET_ESP8266
    gpio_set_level(GPIO_NUM_2, 0);
#endif
}

#if 1
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

    (void) uart_write_bytes(UART_NUM_0, (const char*)stk500v2_seq, sizeof(stk500v2_seq));
    vTaskDelay(50 / portTICK_PERIOD_MS);
    (void) uart_read_bytes(UART_NUM_0, uart_rx_buffer, uart_rx_buffer_size, 10);

    ESP_ERROR_CHECK(uart_set_baudrate(UART_NUM_0, orig_baudrate));
}
#endif
