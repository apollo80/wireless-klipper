/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 * @detauls WiFi functions
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wireless_klipper_sta.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>


/// @brief initializes WiFi and starts the process of connecting to an access point
/// @param[in] ssid name of WiFi access point
/// @param[in] password WiFi connection password
void init_wifi()
{
#if ENABLE_DEBUG
    Serial.println();
    Serial.printf("-- init begin --");
    Serial.println();

    Serial.print("Connecting to ");
    Serial.print(moduleSettings.wifi_ssid);
    Serial.println();
#endif

    /*
     * Explicitly set the ESP8266 as a WiFi client, otherwise it will default
     * to acting as both a client and an access point, which can cause network problems
     * with your other WiFi devices on your WiFi network 
     */
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.setPhyMode(WIFI_PHY_MODE_11N);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);

    if (moduleSettings.wifi_hostname[0])
    {
        WiFi.hostname(moduleSettings.wifi_hostname);
    }
    WiFi.begin(moduleSettings.wifi_ssid, moduleSettings.wifi_password);


    // establish a wifi connection to the server
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(200);
#if ENABLE_DEBUG
        Serial.print(".");
#endif
    }

#if ENABLE_DEBUG
    Serial.println();
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
#endif


    // Initialize the LED_BUILTIN pin as an output
    pinMode(LED_BUILTIN, OUTPUT);

    // signal that we are loaded
    digitalWrite(LED_BUILTIN, HIGH); delay(200); digitalWrite(LED_BUILTIN, LOW); delay(200);
    digitalWrite(LED_BUILTIN, HIGH); delay(200); digitalWrite(LED_BUILTIN, LOW); delay(200);
    digitalWrite(LED_BUILTIN, HIGH); delay(200); digitalWrite(LED_BUILTIN, LOW); delay(200);
    digitalWrite(LED_BUILTIN, HIGH);

    // init mDNS service
    if (moduleSettings.wifi_hostname[0])
    {
        bool mDNS_init = MDNS.begin(moduleSettings.wifi_hostname);
        MDNS.addService("http", "tcp", 80);
#if ENABLE_DEBUG
        if (mDNS_init)
        {
            Serial.println("mDNS responder started");
        }
#endif
    }


#if ENABLE_DEBUG
    Serial.printf("-- init complete --");
    Serial.println();
#endif
}


void wifi_update()
{
    MDNS.update();
}