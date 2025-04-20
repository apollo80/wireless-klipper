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

#if CONFIG_IDF_TARGET_ESP32C2
#   include <soc/uart_pins.h>
#endif
#include <driver/uart.h>
#include "driver/gpio.h"

#include <sys/cdefs.h>
#include <esp_log.h>


static const char *TAG = "wk bridge";
QueueHandle_t uart0_queue;

static uart_config_t uart_config = {
    .baud_rate    = 250000                      /*!< UART baud rate */
    , .data_bits  = UART_DATA_8_BITS            /*! UART byte size */
    , .parity     = UART_PARITY_DISABLE         /*!< UART parity mode */
    , .stop_bits  = UART_STOP_BITS_1            /*!< UART stop bits */
    , .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE    /*!< UART HW flow control mode (cts/rts) */
#if CONFIG_IDF_TARGET_ESP32C2
    , .source_clk = UART_SCLK_DEFAULT
#elif CONFIG_IDF_TARGET_ESP8266
    , .rx_flow_ctrl_thresh = 122
#endif
};

static struct bridge_config_t bridge_config;
static SemaphoreHandle_t      bridge_config_mutex = NULL;

void udp_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *pbuf,const ip_addr_t *ip_addr, uint16_t port);
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

    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, rx_buffer_size, tx_buffer_size, 128, &uart0_queue, 0));

#if CONFIG_IDF_TARGET_ESP32C2
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, U0TXD_GPIO_NUM, U0RXD_GPIO_NUM, U0RTS_GPIO_NUM, U0CTS_GPIO_NUM));
    ESP_ERROR_CHECK(uart_set_mode(UART_NUM_0, UART_MODE_UART));
#endif
}

void uart2net_start() {
    // create UART task
    xTaskCreate(task_uart2net, "uart2net_task", 2048, &bridge_config, 6, &(bridge_config.task__uart2net));
    configASSERT(bridge_config.task__uart2net);
}

void net2uart_start() {
    if (bridge_config_mutex == NULL) {
        bridge_config_mutex = xSemaphoreCreateMutex();
    }

    bridge_config.app_config = app_config();

    bridge_config.uart_rx_buffer = (uint8_t *) malloc(bridge_config.app_config->uart_rx_buffer_size);
    bridge_config.send_buffer    = (uint8_t *) malloc(bridge_config.app_config->uart_rx_buffer_size
            + sizeof(struct msg_header_t) + sizeof(uint16_t));

    bridge_config.msg_net_index  = 0;
    bridge_config.msg_uart_index = 0;

    bridge_config.timer__session = xTimerCreate("udp_session", 4000 / portTICK_PERIOD_MS, true, NULL, udp_session_done);
    bridge_config.task__uart2net = NULL;
    bridge_config.sem__net2uart  = xSemaphoreCreateBinary();

    bridge_config.udp_socket = udp_new();
    if (!bridge_config.udp_socket) {
        ESP_LOGE(TAG, "failed create udp socket - no memory");
        return;
    }
#if CONFIG_IDF_TARGET_ESP32C2
    bridge_config.source_address.addr = 0;
#elif CONFIG_IDF_TARGET_ESP8266
    bridge_config.source_address.u_addr.ip4.addr = 0;
#else
#error Unknown IDF_TARGET
#endif
    bridge_config.source_port = 0;

    err_t ret = udp_bind(bridge_config.udp_socket, IP4_ADDR_ANY, bridge_config.app_config->net_port);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed bind udp socket: ret %i", ret);
        return;
    }
    udp_recv(bridge_config.udp_socket, udp_recv_cb, &bridge_config);
}

bool net2uart_is_started() {
    return (bridge_config.udp_socket != NULL);
}


void udp_session_done(TimerHandle_t timer_handle)
{
    ESP_LOGI(TAG, "!! udp_session_done !!");

    // ??? -> bridge_uart2net__stop(&bridge_config);
    webctrl_start();

#if CONFIG_IDF_TARGET_ESP8266
    gpio_set_level(GPIO_NUM_2, 1);
#endif

    xTimerStop(bridge_config.timer__session, 10);
}

#if CONFIG_WK_UDP_LOG_ENABLE

void udp_log(int udp_socket, struct sockaddr_in* dest_addr, const char* format, ...) {
    static uint8_t udp_message[1024 + 16];
    static char*   udp_message_buf = (char*)(udp_message + sizeof(struct msg_header_t));
    static struct  msg_header_t* log_msg_header = (struct msg_header_t *) (udp_message);

    log_msg_header->msg_prefix = prefix_udpLog;
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
    log_msg_header->msg_size = htons(msg_size);

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
}
#endif
