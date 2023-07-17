/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 * @detauls settings functions
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wireless_klipper_sta.h"
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
    if(moduleSettings.version.major != tmp_settings.version.major)
    {
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
    Serial.println("Writing configuration to EEPROM:");
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
    Serial.printf("  - version                  : %i.%i.%i-%i\n"
        , cfg.version.major
        , cfg.version.minor
        , cfg.version.revision
        , cfg.version.bugfix);

    cfg.wifi_ssid[sizeof(moduleSettings.wifi_ssid) - 1] = 0;
    cfg.wifi_password[sizeof(moduleSettings.wifi_password) - 1] = 0;

    Serial.printf("  - hostname                 : \"%s\"\n", cfg.wifi_hostname);
    Serial.printf("  - SSID name                : \"%s\"\n", cfg.wifi_ssid);
    Serial.printf("  - SSID password            : \"%s\"\n", cfg.wifi_password);
    Serial.printf("  - WiFi mode                : \"%s\"\n", (cfg.wifi_use_sta ? "sta" : "ap"));
    Serial.println();
    Serial.printf("  - serial port baud         : %i\n", cfg.serialPort_baud);
    Serial.printf("  - serial port rxBuffer size: %i\n", cfg.serialPort_rxBuffSize);
    Serial.println();
    Serial.printf("  - tcp server port          : %i\n", cfg.tcpServer_port);
    Serial.printf("  - tcp server buffers size  : %i (x2)\n", cfg.tcpServer_buffSize);
    Serial.println();
    Serial.printf("  - use static ip            : %s\n", (cfg.use_static_ip ? "true" : "false"));
    Serial.printf("  - static IP address        : %i.%i.%i.%i\n"
        , cfg.static_IPaddress[0], cfg.static_IPaddress[1], cfg.static_IPaddress[2], cfg.static_IPaddress[3] );
    Serial.printf("  - static netmask           : %i.%i.%i.%i\n"
        , cfg.static_netmask[0], cfg.static_netmask[1], cfg.static_netmask[2], cfg.static_netmask[3] );
    Serial.printf("  - static gateway           : %i.%i.%i.%i\n"
        , cfg.static_gateway[0], cfg.static_gateway[1], cfg.static_gateway[2], cfg.static_gateway[3] );
}
#endif
