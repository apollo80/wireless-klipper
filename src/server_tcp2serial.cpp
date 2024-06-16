/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 * @detauls tcp2serial functions
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wireless_klipper.h"
#include <WiFiClient.h>
#include <WiFiServer.h>


// listening tcp server object
static WiFiServer tcpServer(moduleSettings.tcpServer_port);

// single client object
// since there is only one UART, then only one tcp client is needed.
static WiFiClient tcpClient;

// buffer for receiving data from a tcp client
static uint8_t* tcp2serial_buf;
static uint8_t* tcp2serial_buf_ptr;
static uint16_t tcp2serial_bufSize = 0;

// buffer for receiving data to tcp client
static uint8_t* serial2tcp_buf;
static uint16_t serial2tcp_bufSize = 0;
static unsigned long serial2tcp_lastReadTime = 0;

static bool existClient_tcp2serial = false;


static bool read_tcp2buffer();
static bool read_serial2buffer();

static bool write_buffer2serial();
static bool write_buffer2tcp();


void init_tcp2serial()
{
    tcp2serial_buf = tcp2serial_buf_ptr = (uint8_t*)calloc(moduleSettings.tcpServer_buffSize, sizeof(uint8_t));
    serial2tcp_buf = (uint8_t*)calloc(moduleSettings.tcpServer_buffSize, sizeof(uint8_t));

    tcp2serial_bufSize = 0;
    serial2tcp_bufSize = 0;
    serial2tcp_lastReadTime = 0;

    // start bridge server
    // tcpServer.setNoDelay(true);
    tcpServer.begin(moduleSettings.tcpServer_port);
    serial2tcp_lastReadTime = false;

    // WiFiClient::setDefaultNoDelay(true);
    // WiFiClient::setDefaultSync(true);
}

bool handle_tcp2serial()
{
    // check the connection with the current client
    // i.e. if there is a client, but there is no connection with it,
    // then we close / stop the client.
    if (!tcpClient) {
        if (existClient_tcp2serial) {
#if ENABLE_DEBUG
            DEBUG_ESP_PORT.printf("(%9lu) tcpClient - %i\n", millis(), tcpClient.status());
            DEBUG_ESP_PORT.printf("(%9lu) disconnect\n", millis());
#endif
            tcpClient.stop(1);
            existClient_tcp2serial = false;
            return existClient_tcp2serial;
        }

        // check for new connections
        if (!tcpServer.hasClient()) {
            return existClient_tcp2serial;
        }

        // if the client is waiting in the queue
        tcpClient = tcpServer.accept();
        //tcpClient.keepAlive(1, 1, 9);

        existClient_tcp2serial = true;

#if ENABLE_DEBUG
        DEBUG_ESP_PORT.printf("(%9lu) new connection: %i.%i.%i.%i:%i\n", millis(), tcpClient.remoteIP()[0], tcpClient.remoteIP()[1], tcpClient.remoteIP()[2], tcpClient.remoteIP()[3], tcpClient.remotePort());
#endif
    }

    do {
        // check for data on the serial port.
        // if the serial port is ready for reading (there is data),
        // then we subtract them
        if (read_serial2buffer())
            break;

        // if the client is ready to transfer data, then transfer this data
        if (write_buffer2tcp())
            break;


        // we check the presence of input data from the tcp client.
        // subtract the input
        if (read_tcp2buffer())
            break;

        // if the serial port is ready for writing,
        // then we send read data to it
        if (write_buffer2serial())
            break;

    } while (0);

    tcpClient.flush(10);
    return existClient_tcp2serial;
}


bool read_tcp2buffer()
{
    if (tcp2serial_buf != tcp2serial_buf_ptr) {
        return false;
    }

    int availableBytes = tcpClient.available();
    if (availableBytes == 0) {
        return false;
    }

    // calculate how much space is left in the buffer
    int leftSize_tcp2serial = moduleSettings.tcpServer_buffSize - tcp2serial_bufSize;

    // if there is nothing left - exit
    if (leftSize_tcp2serial == 0) {
        return false;
    }

    // read the input
    uint8_t* bufOffset = tcp2serial_buf + tcp2serial_bufSize;
    size_t needReading = std::min(availableBytes, leftSize_tcp2serial);
    int readedBytes = tcpClient.read(bufOffset, needReading);

#if ENABLE_DEBUG
    DEBUG_ESP_PORT.printf("(%9lu) re_tcp2buf: read %i bytes:\nrecv: ", millis(), readedBytes);
    DEBUG_ESP_PORT.write(bufOffset, readedBytes);
#endif

    tcp2serial_bufSize += readedBytes;

#if ENABLE_DEBUG
    DEBUG_ESP_PORT.printf("\n(%9lu) rc_tcp2buf: buf size %i\n", millis(), tcp2serial_bufSize);
#endif

    if ((tcp2serial_bufSize + serial2tcp_bufSize) > 0) {
        // turn on the indication of the presence of data in the buffer
        digitalWrite(LED_BUILTIN, LOW);
    }

    return true;
}

bool read_serial2buffer()
{
    int availableBytes = Serial.available();
    if (availableBytes == 0) {
        return false;
    }

    if (serial2tcp_lastReadTime == 0) {
        serial2tcp_lastReadTime = millis();
    }

    // calculate how much space is left in the buffer
    int leftSize_serial2tcp = moduleSettings.tcpServer_buffSize - serial2tcp_bufSize;

    // if there is nothing left - exit
    if (leftSize_serial2tcp == 0) {
#if ENABLE_DEBUG
        DEBUG_ESP_PORT.printf("(%9lu) rd_ser2buf: leftSize_serial2tcp == 0\n", millis());
#endif
        return false;
    }

    // read the input
    uint8_t* bufOffset = serial2tcp_buf + serial2tcp_bufSize;
    size_t needReading = std::min(availableBytes, leftSize_serial2tcp);
#if ENABLE_DEBUG
    DEBUG_ESP_PORT.printf("(%9lu) rd_ser2buf: needReading = %i\n", millis(), availableBytes);
#endif

    int readedBytes = Serial.read(bufOffset, needReading);
    serial2tcp_bufSize += readedBytes;

#if ENABLE_DEBUG
    DEBUG_ESP_PORT.printf("(%9lu) rd_ser2buf: buf size: %i (readed %i bytes)\n", millis(), serial2tcp_bufSize, readedBytes);
#endif
    return true;
}

bool write_buffer2serial()
{
    static size_t availableForWrite_count = 0;

    // check the presence of data in the buffer from the tcp client
    if (tcp2serial_bufSize == 0) {
        return false;
    }

    // if the serial port is not ready for recording - skip recording
    int availableForWrite = Serial.availableForWrite();
    if (availableForWrite < 32) {
        availableForWrite_count++;

        if (availableForWrite_count < 20) {
            return false;
        }

        availableForWrite_count = 0;
    }

#if ENABLE_DEBUG
    DEBUG_ESP_PORT.printf("(%9lu) wr_buf2ser: need %i; avw %i:\ndata: ", millis(), tcp2serial_bufSize, availableForWrite);
#endif

#if 0
    // if the serial port is ready for writing, then we send read data to it
    size_t writedBytes = Serial.write(tcp2serial_buf, std::min(size_t(tcp2serial_bufSize), size_t(availableForWrite)));
    if (writedBytes) {
        tcp2serial_buf = (uint8_t*)os_memmove(tcp2serial_buf, tcp2serial_buf + writedBytes, tcp2serial_bufSize - writedBytes);
        tcp2serial_bufSize -= writedBytes;
    }
#else
    // if the serial port is ready for writing, then we send read data to it
    size_t writedBytes = Serial.write(tcp2serial_buf_ptr, std::min(size_t(tcp2serial_bufSize), size_t(availableForWrite)));
    tcp2serial_buf_ptr += writedBytes;
    tcp2serial_bufSize -= writedBytes;

    if (tcp2serial_bufSize == 0) {
        tcp2serial_buf_ptr = tcp2serial_buf;
    }
#endif

#if ENABLE_DEBUG
    DEBUG_ESP_PORT.printf("\n(%9lu) wr_buf2ser: write %i bytes\n", millis(), writedBytes);
    DEBUG_ESP_PORT.printf("(%9lu) wr_buf2ser: tcp2ser buf size %i\n", millis(), tcp2serial_bufSize);
#endif

    return true;
}

bool write_buffer2tcp()
{
    // check the presence of data in the buffer from the tcp client
    if (serial2tcp_bufSize == 0) {
        return false;
    }

    if (serial2tcp_bufSize < moduleSettings.tcpServer_buffSize
        && (millis() - serial2tcp_lastReadTime) < 5) {
#if ENABLE_DEBUG
        DEBUG_ESP_PORT.printf("(%9lu) wr_buf2tcp: serial2tcp_lastReadTime < 5\n", millis());
#endif
        return false;
    }

    // if tcp client is not ready for recording - skip recording
    int availableForWrite = tcpClient.availableForWrite();
#if ENABLE_DEBUG
    DEBUG_ESP_PORT.printf("(%9lu) wr_buf2tcp: leftSize_tcp(%i)\n", millis(), availableForWrite);
#endif
    if (availableForWrite < 32) {
        return false;
    }

#if ENABLE_DEBUG
    DEBUG_ESP_PORT.printf("(%9lu) wr_buf2tcp: neededSend = %i\n", millis(), serial2tcp_bufSize);
#endif

    // sending the read data from the buffer to the socket
    int writedBytes = tcpClient.write(serial2tcp_buf, std::min(availableForWrite, int(serial2tcp_bufSize)));
    if (writedBytes) {
        serial2tcp_buf = (uint8_t*)os_memmove(serial2tcp_buf, serial2tcp_buf + writedBytes, serial2tcp_bufSize - writedBytes);
        serial2tcp_bufSize -= writedBytes;
        serial2tcp_lastReadTime = 0;
    }

#if ENABLE_DEBUG
    DEBUG_ESP_PORT.printf("(%9lu) wr_buf2tcp: send %i bytes\n", millis(), writedBytes);
    DEBUG_ESP_PORT.printf("(%9lu) wr_buf2tcp: ser2tcp buf size: %i\n", millis(), serial2tcp_bufSize);
#endif

    if ((tcp2serial_bufSize + serial2tcp_bufSize) == 0) {
        // turn off the indication of the presence of data in the buffer
        digitalWrite(LED_BUILTIN, HIGH);
    }

    return true;
}
