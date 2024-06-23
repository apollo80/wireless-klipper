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

#include <sdkconfig.h>
#include <freertos/queue.h>

#include <driver/uart.h>
#include "driver/gpio.h"

#include <sys/cdefs.h>
#include <esp_log.h>


static const char *TAG = "tcp2uart";
QueueHandle_t uart0_queue;

static uart_config_t uart_config = {
    .baud_rate   = 74880                       /*!< UART baud rate */
    , .data_bits = UART_DATA_8_BITS            /*! UART byte size */
    , .parity    = UART_PARITY_DISABLE         /*!< UART parity mode */
    , .stop_bits = UART_STOP_BITS_1            /*!< UART stop bits */
    , .flow_ctrl = UART_HW_FLOWCTRL_DISABLE    /*!< UART HW flow control mode (cts/rts) */
    , .rx_flow_ctrl_thresh = UART_FIFO_LEN     /*!< UART HW RTS threshold */

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

void tcp2uart_init() {
    uart_config.baud_rate = app_config()->uart_baud_rate;

    uart_param_config(UART_NUM_0, &uart_config);

    // Install UART driver, and get the queue.
    int rx_buffer_size = (int)(app_config()->uart_rx_buffer_size);
    int tx_buffer_size = rx_buffer_size;

    uart_driver_install(UART_NUM_0, rx_buffer_size, tx_buffer_size, 100, &uart0_queue, 0);
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

    xTaskCreate(bridge_udp2uart, "net2uart", 2048, &bridge_config, 5, &(bridge_config.task__net2uart));
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

    xTimerStop(timer_handle, 0);
    vTaskDelete(bridge_config.task__uart2net);

    bridge_config_lock();
    {
        memset(&(bridge_config.source_address), 0, bridge_config.source_address_len);
        bridge_config.source_address_len = 0;

        bridge_config.msg_net_index = 0;
        bridge_config.msg_uart_index = 0;

        bridge_config.task__uart2net = NULL;
    }
    bridge_config_unlock();

    gpio_set_level(GPIO_NUM_2, 1);
}
