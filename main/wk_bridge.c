/*
 * @file
 * @brief esp32c2 uart2net bridge for klipper
 * @details settings functions
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wk_tasks.h"
#include "wk_bridge.h"

#include <sdkconfig.h>
#include <freertos/queue.h>

#include <soc/uart_pins.h>
#include <driver/uart.h>
#include "driver/gpio.h"

#include <sys/cdefs.h>
#include <esp_log.h>


static const char *TAG = "tcp2uart";
QueueHandle_t uart0_queue;

static uart_config_t uart_config = {
    .baud_rate    = 250000                      /*!< UART baud rate */
    , .data_bits  = UART_DATA_8_BITS            /*! UART byte size */
    , .parity     = UART_PARITY_DISABLE         /*!< UART parity mode */
    , .stop_bits  = UART_STOP_BITS_1            /*!< UART stop bits */
    , .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE    /*!< UART HW flow control mode (cts/rts) */
#if CONFIG_IDF_TARGET_ESP32C2
    , .source_clk = UART_SCLK_DEFAULT
#else
    , .rx_flow_ctrl_thresh = 122
#endif
};

static struct bridge_config_t bridge_config;
static SemaphoreHandle_t      bridge_config_mutex = NULL;

void udp_session_done(TimerHandle_t timer_handle);

void bridge_config_lock() {
    if (bridge_config_mutex) {
        xSemaphoreTake(bridge_config_mutex, 100 / portTICK_PERIOD_MS );
    }
}
void bridge_config_unlock() {
    if (bridge_config_mutex) {
        xSemaphoreGive(bridge_config_mutex);
    }
}

void uart_init() {
    uart_config.baud_rate = app_config()->uart_baud_rate;

    // Install UART driver, and get the queue.
    int rx_buffer_size = (int)(app_config()->uart_rx_buffer_size);
    int tx_buffer_size = rx_buffer_size;

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, rx_buffer_size, tx_buffer_size, 20, &uart0_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
#if CONFIG_IDF_TARGET_ESP32C2
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, U0TXD_GPIO_NUM, U0RXD_GPIO_NUM, U0RTS_GPIO_NUM, U0CTS_GPIO_NUM));
    ESP_ERROR_CHECK(uart_set_mode(UART_NUM_0, UART_MODE_UART));
#endif
}

void uart_reinit() {
    ESP_ERROR_CHECK(uart_driver_delete(UART_NUM_0));
    uart_init();
}


void tcp2uart_start() {
    ESP_LOGI(TAG, "tcp2uart start");

    if (bridge_config_mutex == NULL) {
        bridge_config_mutex = xSemaphoreCreateMutex();
    }

    bridge_config.app_config = app_config();
    bridge_config.socket = -1;

    bridge_config.source_address_len = sizeof(bridge_config.source_address);
    memset(&(bridge_config.source_address), 0, bridge_config.source_address_len);

    bridge_config.net_rx_buffer  = (uint8_t *) malloc(bridge_config.app_config->net_rx_buffer_size);
    bridge_config.uart_rx_buffer = (uint8_t *) malloc(bridge_config.app_config->uart_rx_buffer_size);

    bridge_config.msg_net_index  = 0;
    bridge_config.msg_uart_index = 0;

    bridge_config.timer__session = xTimerCreate("udp_session", 10000 / portTICK_PERIOD_MS, true, NULL, udp_session_done);

    bridge_config.task__net2uart = NULL;
    bridge_config.sem__net2uart  = xSemaphoreCreateBinary();

    bridge_config.task__uart2net = NULL;
    bridge_config.sem__uart2net  = xSemaphoreCreateBinary();

    xTaskCreate(bridge_net2uart, "net2uart", 2048, &bridge_config, 5, &(bridge_config.task__net2uart));
}

void tcp2uart_stop() {
    ESP_LOGI(TAG, "tcp2uart stop");

    if (bridge_config.task__uart2net) {
        vTaskDelete(bridge_config.task__uart2net);
        bridge_config.task__uart2net = NULL;
    }

    if (bridge_config.task__net2uart) {
        vTaskDelete(bridge_config.task__net2uart);
        bridge_config.task__net2uart = NULL;
    }

    if (bridge_config.net_rx_buffer) {
        free(bridge_config.net_rx_buffer);
        bridge_config.net_rx_buffer = NULL;
    }

    if (bridge_config.uart_rx_buffer) {
        free(bridge_config.uart_rx_buffer);
        bridge_config.uart_rx_buffer = NULL;
    }

    // bridge_config.net_session_started = false;

    if (bridge_config.timer__session) {
        xTimerDelete(bridge_config.timer__session, 0);
    }

    if (bridge_config.sem__net2uart) {
        vSemaphoreDelete(bridge_config.sem__net2uart);
        bridge_config.sem__net2uart = NULL;
    }

    if (bridge_config.sem__uart2net) {
        vSemaphoreDelete(bridge_config.sem__uart2net);
        bridge_config.sem__uart2net = NULL;
    }

    gpio_set_level(GPIO_NUM_2, 1);
    if (bridge_config_mutex) {
        vSemaphoreDelete(bridge_config_mutex);
        bridge_config_mutex = NULL;
    }
}

void udp_session_done(TimerHandle_t timer_handle)
{
    ESP_LOGI(TAG, "!! udp_session_done !!");

    bridge_uart2net__stop(&bridge_config);

#if CONFIG_IDF_TARGET_ESP8266
    gpio_set_level(GPIO_NUM_2, 1);
#endif
}

#if CONFIG_WK_UDP_LOG_ENABLE

void udp_log(int udp_socket, struct sockaddr_in* dest_addr, const char* format, ...) {
    static uint8_t udp_message[1024 + 16];
    static char*   udp_message_buf = (char*)(udp_message + sizeof(struct msg_header_t));
    static struct  msg_header_t* log_msg_header = (struct msg_header_t *) (udp_message);

    log_msg_header->msg_prefix = htonl(prefix_udpLog);
    log_msg_header->uart_index = 0;
    log_msg_header->net_index = 0;
    log_msg_header->msg_size = 0;

    int msg_size = 0;
    {
        va_list args;
        va_start(args, format);
        msg_size = vsnprintf(udp_message_buf, 1024, format, args);
        va_end(args);
    }
    log_msg_header->msg_size = htonl(msg_size);

    bridge_config_lock();
    ssize_t sent_bytes = 0;
    if (bridge_config.socket != -1) {
        sent_bytes = sendto(bridge_config.socket
            , udp_message, sizeof(struct msg_header_t) + msg_size
            , MSG_DONTWAIT, (struct sockaddr *)&(bridge_config.source_address), bridge_config.source_address_len);
    }
    bridge_config_unlock();

    if (sent_bytes < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        return;
    }
    assert(sent_bytes == (sizeof(struct  msg_header_t) + msg_size));
    // ESP_LOGI(TAG, "Message sent");
}
#endif