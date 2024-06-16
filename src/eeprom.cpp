/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 * @detauls settings functions
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wireless_klipper.h"
#include <EEPROM.h>


static settings_t tmp_settings;


/// @brief read setting from EEPROM
bool read_settings()
{
    memset(&tmp_settings, 255, sizeof(settings_t));

    EEPROM.begin(sizeof(settings_t));
    delay(10);
    EEPROM.get(0, tmp_settings);
    EEPROM.end();

    // @todo implement settings update when version is updated.
    if (moduleSettings.version.major != tmp_settings.version.major) {
        return false;
    }

    memcpy(&moduleSettings, &tmp_settings, sizeof(settings_t));
    memset(&tmp_settings, 255, sizeof(settings_t));

    return true;
}

/// @brief write setting to EEPROM
void write_settings(settings_t& cfg)
{
#if ENABLE_DEBUG
    DEBUG_ESP_PORT.printf("(%9lu) Writing configuration to EEPROM:", millis());
    printModuleSettings(cfg);
#endif

    EEPROM.begin(sizeof(settings_t));
    delay(10);
    EEPROM.put(0, cfg);
    EEPROM.commit();
    EEPROM.end();
}

#if ENABLE_DEBUG
void printModuleSettings(settings_t& cfg)
{
    DEBUG_ESP_PORT.printf("(%9lu)  - version                  : %i.%i.%i-%i\n", millis(), cfg.version.major, cfg.version.minor, cfg.version.revision, cfg.version.bugfix);

    cfg.wifi_ssid[sizeof(moduleSettings.wifi_ssid) - 1] = 0;
    cfg.wifi_password[sizeof(moduleSettings.wifi_password) - 1] = 0;

    DEBUG_ESP_PORT.printf("(%9lu)  - hostname                 : \"%s\"\n", millis(), cfg.wifi_hostname);
    DEBUG_ESP_PORT.printf("(%9lu)  - SSID name                : \"%s\"\n", millis(), cfg.wifi_ssid);
    DEBUG_ESP_PORT.printf("(%9lu)  - SSID password            : \"%s\"\n", millis(), cfg.wifi_password);
    DEBUG_ESP_PORT.printf("(%9lu)  - WiFi mode                : \"%s\"\n", millis(), (cfg.wifi_use_sta ? "sta" : "ap"));
    DEBUG_ESP_PORT.println();
    DEBUG_ESP_PORT.printf("(%9lu)  - serial port baud         : %i\n", millis(), cfg.serialPort_baud);
    DEBUG_ESP_PORT.printf("(%9lu)  - serial port rxBuffer size: %i\n", millis(), cfg.serialPort_rxBuffSize);
    DEBUG_ESP_PORT.println();
    DEBUG_ESP_PORT.printf("(%9lu)  - tcp server port          : %i\n", millis(), cfg.tcpServer_port);
    DEBUG_ESP_PORT.printf("(%9lu)  - tcp server buffers size  : %i (x2)\n", millis(), cfg.tcpServer_buffSize);
    DEBUG_ESP_PORT.println();
    DEBUG_ESP_PORT.printf("(%9lu)  - use static ip            : %s\n", millis(), (cfg.use_static_ip ? "true" : "false"));
    DEBUG_ESP_PORT.printf("(%9lu)  - static IP address        : %i.%i.%i.%i\n", millis(), cfg.static_IPaddress[0], cfg.static_IPaddress[1], cfg.static_IPaddress[2], cfg.static_IPaddress[3]);
    DEBUG_ESP_PORT.printf("(%9lu)  - static netmask           : %i.%i.%i.%i\n", millis(), cfg.static_netmask[0], cfg.static_netmask[1], cfg.static_netmask[2], cfg.static_netmask[3]);
    DEBUG_ESP_PORT.printf("(%9lu)  - static gateway           : %i.%i.%i.%i\n", millis(), cfg.static_gateway[0], cfg.static_gateway[1], cfg.static_gateway[2], cfg.static_gateway[3]);
}
#endif
