/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 *
 * author: apollo80
 * @email: apollo80@list.ru

 * test: socat -dd pty,link=/tmp/virtualcom,ispeed=b250000,ospeed=b250000,raw,echo=0 TCP4-CONNECT:192.168.4.1:8888,nodelay,forever,interval=1
 */

#include "wireless_klipper_sta.h"

/// @brief firmware_version
version_t firmware_version = { 0, 0, 2, 1 };

/// @brief default configuration
struct settings_t moduleSettings
{
    /// @brief firmware version
    .version = { 0, 0, 2, 1 },

    /// @brief wifi point name
    "esp8266",

    /// @brief wifi SSID
    "MKS Robin WiFi",

    /// @brief wifi password
    "12345678",

    /// @brief wifi mode
    .wifi_use_sta = true,


    // default values for the serial port
    .serialPort_baud = 250000,

    .serialPort_rxBuffSize = 256,


    /// @brief port of tcp2serial server
    .tcpServer_port = 8888,

    // buffer size for receiving/transmitting data
    .tcpServer_buffSize = 256,


    /// @brief
    .use_static_ip = false,

    /// @brief
    .static_IPaddress = { 192, 168, 4, 100 },

    /// @brief
    .static_netmask = { 255, 255, 255, 0 },

    /// @brief
    .static_gateway = { 192, 168, 4, 1 },
};


void setup()
{

    // read modules settings from EEPROM
    bool subsequentLaunch = read_settings();
    if(!subsequentLaunch)
    {
        write_settings(moduleSettings);
    }


    // init serial
    Serial.begin(moduleSettings.serialPort_baud);
    Serial.setRxBufferSize(moduleSettings.serialPort_rxBuffSize);

#if ENABLE_DEBUG
    delay(50);
    Serial.println();
    Serial.println("Configuration (from EEPROM):");
    printModuleSettings(moduleSettings);
#endif

    // initialize and start tcp2serial server
    init_tcp2serial();

    // initialize and start http server
    init_httpServer();

    // initialize WiFi and start the process of connecting to an access point
    // waiting for a Wi-Fi connection
    init_wifi();
}

// the loop function runs over and over again forever
void loop()
{
    wifi_update();

    bool existClient_tcp2serial = handle_tcp2serial();
    if(!existClient_tcp2serial)
    {
        handle_httpServer();
    }
}
