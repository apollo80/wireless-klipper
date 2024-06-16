/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 *
 * author: apollo80
 * @email: apollo80@list.ru
 */

#include <Arduino.h>
#include "wireless_klipper.h"


/// @brief firmware_version
version_t firmware_version = { 0, 0, 3, 2 };

/// @brief default configuration
struct settings_t moduleSettings {
    /// @brief firmware version
    .version = { 0, 0, 3, 2 }

    /// @brief wifi point name
    , "esp8266"

    /// @brief wifi SSID
    // "MKS Robin WiFi",
    , "apollonetwork"

    /// @brief wifi password
    , "vfnbkmlf"


    /// @brief wifi mode
    , .wifi_use_sta = true


    // default values for the serial port
    , .serialPort_baud = 250000
    , .serialPort_rxBuffSize = 512

    /// @brief port of tcp2serial server
    , .tcpServer_port = 8888

    // buffer size for receiving/transmitting data
    , .tcpServer_buffSize = 512


    /// @brief Sign of using static network addressing
    , .use_static_ip = false

    /// @brief static network addressing module
    , .static_IPaddress = { 192, 168, 4, 100 }

    /// @brief network module when using static network addressing
    , .static_netmask = { 255, 255, 255, 0 }

    /// @brief gateway address when using static addressing
    , .static_gateway = { 192, 168, 4, 1 }
};


void setup()
{

    // read modules settings from EEPROM
    bool subsequentLaunch = read_settings();
    if (!subsequentLaunch) {
        write_settings(moduleSettings);
    }


    // init serial
    Serial.setRxBufferSize(moduleSettings.serialPort_rxBuffSize);
    Serial.begin(moduleSettings.serialPort_baud, SERIAL_8N1, SERIAL_FULL);

#if ENABLE_DEBUG
    delay(50);
    //Serial.setDebugOutput(true);

    DEBUG_ESP_PORT.printf("(%9lu)Configuration (from EEPROM):", millis());
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
    bool existClient_tcp2serial = handle_tcp2serial();
    if (!existClient_tcp2serial) {
        handle_httpServer();
    }

#if ENABLE_DEBUG
    static uint32_t lastmin = 65536;
    static uint32_t memfree;
    static uint32_t memmax;
    static uint8_t memfrag;

    ESP.getHeapStats(&memfree, &memmax, &memfrag);
    if (lastmin > memfree) {
        DEBUG_ESP_PORT.printf("(%9lu) -> free: %5d - max: %5d - frag: %3d%% <- \n", millis(), memfree, memmax, memfrag);
        //syslog(LOG_SYSLOG|LOG_INFO, "(%9lu) -> free: %5d - max: %5d - frag: %3d%% <-", millis(), memfree, memmax, memfrag);

        lastmin = memfree;
    }
#endif

    wifi_update(existClient_tcp2serial);
}
