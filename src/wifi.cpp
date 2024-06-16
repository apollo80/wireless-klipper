/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 * @detauls WiFi functions
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wireless_klipper.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>


/// @brief initializes WiFi and starts the process of connecting to an access point
/// @param[in] ssid name of WiFi access point
/// @param[in] password WiFi connection password
void init_wifi()
{
#if ENABLE_DEBUG
    DEBUG_ESP_PORT.println();
    DEBUG_ESP_PORT.printf("-- init begin --");
    DEBUG_ESP_PORT.println();

    DEBUG_ESP_PORT.print("Connecting to ");
    DEBUG_ESP_PORT.print(moduleSettings.wifi_ssid);
    DEBUG_ESP_PORT.println();
#endif

    /*
     * Explicitly set the ESP8266 as a WiFi client, otherwise it will default
     * to acting as both a client and an access point, which can cause network problems
     * with your other WiFi devices on your WiFi network 
     */
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.setPhyMode(WIFI_PHY_MODE_11N);

    // if (moduleSettings.wifi_use_sta)
    WiFi.mode(WIFI_STA);
    // else
    //    WiFi.mode(WIFI_AP);

    // configuration will be saved into SDK flash area
    if (WiFi.getAutoConnect() != true) {
        // on power-on automatically connects to last used hwAP
        WiFi.setAutoConnect(true);

        // automatically reconnects to hwAP in case it's disconnected
        WiFi.setAutoReconnect(true);
    }

    if (moduleSettings.wifi_hostname[0]) {
        WiFi.hostname(moduleSettings.wifi_hostname);
    }

    // if (moduleSettings.wifi_use_sta)
    // {
    WiFi.begin(moduleSettings.wifi_ssid, moduleSettings.wifi_password);
    // }
    // else
    //    WiFi.softAP(moduleSettings.wifi_ssid, moduleSettings.wifi_password);


    // establish a wifi connection to the server
    while (WiFi.status() != WL_CONNECTED) {
        delay(200);
        yield();
#if ENABLE_DEBUG
        DEBUG_ESP_PORT.print(".");
#endif
    }

#if ENABLE_DEBUG
    DEBUG_ESP_PORT.println();
    DEBUG_ESP_PORT.println("WiFi connected");
    DEBUG_ESP_PORT.print("IP address: ");
    DEBUG_ESP_PORT.println(WiFi.localIP());
#endif


    // Initialize the LED_BUILTIN pin as an output
    pinMode(LED_BUILTIN, OUTPUT);

    // signal that we are loaded
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);

    // init mDNS service
    if (moduleSettings.wifi_hostname[0]) {
        MDNS.addService("http", "tcp", 80);

#if ENABLE_DEBUG
        bool mDNS_init = MDNS.begin(moduleSettings.wifi_hostname);
        if (mDNS_init) {
            DEBUG_ESP_PORT.println("mDNS responder started");
        }
#else
        MDNS.begin(moduleSettings.wifi_hostname);
#endif
    }

#if ENABLE_DEBUG
    DEBUG_ESP_PORT.printf("-- init complete --");
    DEBUG_ESP_PORT.println();
#endif
}


void wifi_update(bool existClient)
{
    // restart MDNS
    if (existClient) {
        if (MDNS.isRunning()) {
            MDNS.end();
        }
    } else {
        if (MDNS.isRunning()) {
            MDNS.update();
        } else {
#if ENABLE_DEBUG
            bool mDNS_init = MDNS.begin(moduleSettings.wifi_hostname);
            if (mDNS_init) {
                DEBUG_ESP_PORT.println("mDNS responder started");
            }
#else
            MDNS.begin(moduleSettings.wifi_hostname);
#endif
        }
    }
}
