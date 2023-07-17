/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 * @detauls tcp2serial functions
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wireless_klipper_sta.h"
#include <WiFiClient.h>
#include <WiFiServer.h>


// listening tcp server object
static WiFiServer tcpServer(moduleSettings.tcpServer_port);

// single client object
// since there is only one UART, then only one tcp client is needed.
static WiFiClient tcpClient;

// buffer for receiving data from a tcp client
static uint8_t* tcp2serial_buf;
static uint16_t tcp2serial_bufSize = 0;

// buffer for receiving data to tcp client
static uint8_t* serial2tcp_buf;
static uint16_t serial2tcp_bufSize = 0;

static bool existClient_tcp2serial = false;


static void read_tcp2buffer();
static void read_serial2buffer();

static void write_buffer2serial();
static void write_buffer2tcp();


void init_tcp2serial()
{
    tcp2serial_buf = (uint8_t*)malloc(moduleSettings.tcpServer_buffSize);
    serial2tcp_buf = (uint8_t*)malloc(moduleSettings.tcpServer_buffSize);

    // start bridge server
    tcpServer.setNoDelay(true);
    tcpServer.begin(moduleSettings.tcpServer_port);
}

bool handle_tcp2serial()
{
    // check the connection with the current client
    // i.e. if there is a client, but there is no connection with it,
    // then we close / stop the client.
    if(existClient_tcp2serial && !tcpClient.connected())
    {
#if ENABLE_DEBUG
        Serial.print("disconnect: ");
        printClientIP();
        Serial.println();
#endif

        tcpClient.stop();
        existClient_tcp2serial = false;
        return existClient_tcp2serial;
    }


    // check for new connections
    if (tcpServer.hasClient())
    {
        // if there are no clients yet, then we create a connection
        if(!existClient_tcp2serial)
        {
            tcpClient = tcpServer.accept();
            existClient_tcp2serial = true;

#if ENABLE_DEBUG
            Serial.print("new connection ");
            printClientIP();
            Serial.println();
            printClientStatus();
#endif
        }
        else
        {
#if ENABLE_DEBUG
            Serial.println("reject connection");
#endif

            tcpServer.accept().stop();
        }
    }

    // check for the presence of a connected client,
    // if there are still no clients, then we are waiting for them
    if(!existClient_tcp2serial)
    {
        return existClient_tcp2serial;
    }

    // check for a connected client
    if(!tcpClient.connected())
    {
        return existClient_tcp2serial;
    }

    // we check the presence of input data from the tcp client.
    // subtract the input
    read_tcp2buffer();
    yield();

    // if the serial port is ready for writing,
    // then we send read data to it
    write_buffer2serial();
    yield();

    // check for data on the serial port.
    // if the serial port is ready for reading (there is data),
    // then we subtract them
    read_serial2buffer();
    yield();


    // if the client is ready to transfer data, then transfer this data
    write_buffer2tcp();

    return existClient_tcp2serial;
}


void read_tcp2buffer()
{
    int availableBytes = tcpClient.available();
    if(availableBytes == 0)
    {
        return;
    }

    // calculate how much space is left in the buffer
    int leftSize_tcp2serial = moduleSettings.tcpServer_buffSize - tcp2serial_bufSize;

    // if there is nothing left - exit
    if(leftSize_tcp2serial == 0)
    {
        return;
    }

    // read the input
    uint8_t* bufOffset = tcp2serial_buf + tcp2serial_bufSize;
    size_t needReading = std::min(availableBytes, leftSize_tcp2serial);
    int readedBytes = tcpClient.read(bufOffset, needReading);

#if ENABLE_DEBUG
    Serial.println("read4Client:");
    Serial.write(bufOffset, readedBytes);
    Serial.println("--- end ---");
    Serial.println();
#endif

    tcp2serial_bufSize += readedBytes;

#if ENABLE_DEBUG
    Serial.print("tcp2serial_bufSize: ");
    Serial.println(tcp2serial_bufSize);

    Serial.print("serial2tcp_bufSize: ");
    Serial.println(serial2tcp_bufSize);
#endif

    if((tcp2serial_bufSize + serial2tcp_bufSize) > 0)
    {
        // turn on the indication of the presence of data in the buffer
        digitalWrite(LED_BUILTIN, HIGH);

#if ENABLE_DEBUG
        Serial.println("led on");
#endif
    }
}

void read_serial2buffer()
{
    int availableBytes = Serial.available();
    if(availableBytes == 0)
    {
        return;
    }

    // calculate how much space is left in the buffer
    int leftSize_tcp2serial = moduleSettings.tcpServer_buffSize - tcp2serial_bufSize;

    // if there is nothing left - exit
    if(leftSize_tcp2serial == 0)
    {
        return;
    }

    // read the input
    uint8_t* bufOffset = serial2tcp_buf + serial2tcp_bufSize;
    size_t needReading = std::min(availableBytes, leftSize_tcp2serial);
    int readedBytes = Serial.read(bufOffset, needReading);

#if ENABLE_DEBUG
    Serial.println("read4Serial:");
    Serial.write(bufOffset, readedBytes);
    Serial.println("--- end ---");
    Serial.println();
#endif

    serial2tcp_bufSize += readedBytes;
}

void write_buffer2serial()
{
    // check the presence of data in the buffer from the tcp client
    if(tcp2serial_bufSize == 0)
    {
        return;
    }

    // if the serial port is not ready for recording - skip recording
    if(!Serial.availableForWrite())
    {
        return;
    }

#if ENABLE_DEBUG
    Serial.println("write2Serial:");
#endif

    // if the serial port is ready for writing, then we send read data to it
    int writedBytes = Serial.write(tcp2serial_buf, tcp2serial_bufSize);
    if (writedBytes && writedBytes < tcp2serial_bufSize)
    {
        memmove(tcp2serial_buf, tcp2serial_buf + writedBytes, tcp2serial_bufSize - writedBytes);
    }

    tcp2serial_bufSize -= writedBytes;

#if ENABLE_DEBUG
    Serial.println("--- end ---");
    Serial.println();
#endif
}

void write_buffer2tcp()
{
    // check the presence of data in the buffer from the tcp client
    if(serial2tcp_bufSize == 0)
    {
        return;
    }

    // if tcp client is not ready for recording - skip recording
    if(!tcpClient.availableForWrite())
    {
        return;
    }

    // send the read data from the buffer to the socket
    int writedBytes = tcpClient.write(serial2tcp_buf, serial2tcp_bufSize);
    if(writedBytes && writedBytes < serial2tcp_bufSize)
    {
        memmove(serial2tcp_buf, serial2tcp_buf + writedBytes, serial2tcp_bufSize - writedBytes);
    }

    serial2tcp_bufSize -= writedBytes;

#if ENABLE_DEBUG
    Serial.print("tcp2serial_bufSize: ");
    Serial.println(tcp2serial_bufSize);

    Serial.print("serial2tcp_bufSize: ");
    Serial.println(serial2tcp_bufSize);
#endif

    if((tcp2serial_bufSize + serial2tcp_bufSize) == 0)
    {
        // turn off the indication of the presence of data in the buffer
        digitalWrite(LED_BUILTIN, LOW);

#if ENABLE_DEBUG
        Serial.println("led off");
#endif
    }
}


#if ENABLE_DEBUG
void printClientIP()
{
    Serial.print(tcpClient.remoteIP()[0]);
    Serial.print(".");
    Serial.print(tcpClient.remoteIP()[1]);
    Serial.print(".");
    Serial.print(tcpClient.remoteIP()[2]);
    Serial.print(".");
    Serial.print(tcpClient.remoteIP()[3]);
    Serial.print(":");
    Serial.print(tcpClient.remotePort());
}

void printClientStatus()
{
    Serial.print("client: ");
    if(tcpClient.connected())
    {
        Serial.print("connected");
    }
    else
    {
        Serial.print("not connected");
    }

    Serial.print("; status = ");
    Serial.print(tcpClient.status());
    Serial.println();
}
#endif
