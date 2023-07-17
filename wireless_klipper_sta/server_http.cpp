/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 * @detauls web server functions
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "wireless_klipper_sta.h"
#include "html.h"
#include "ESP8266WebServer.h"


// listening http server object
static ESP8266WebServer httpServer(80);
static char convert_buffer[16];

static void on_root();
static void on_save();
static void on_reboot();
static void on_update();
static void on_update_finish();
static void on_NotFound();

static void toIPAddress(const char* str, uint8_t* ipAddress);
// static String urldecode(String str);
// static unsigned char h2int(char c);

static int baud_range[] = {
    9600
    , 14400
    , 19200
    , 28800
    , 38400
    , 38400
    , 57600
    , 76800
    , 115200
    , 230400
    , 250000
    , 256000
    , 460800
    , 576000
    , 921600
};

uint8_t updaterErrorCode;
String  updaterErrorDesc;

int baud_range_size = 15;
char baud_range_buff[64];
const char baud_range_template[] = "<option value=\"%i\">%i bps</option>";
const char baud_range_selected_template[] = "<option value=\"%i\" selected>%i bps</option>";


void init_httpServer()
{
    httpServer.on("/", HTTP_GET, on_root);
    httpServer.on("/update_config", HTTP_POST, on_save);
    httpServer.on("/restart", HTTP_POST, on_reboot);
    httpServer.on("/upload", HTTP_POST, on_update_finish, on_update);

    // httpServer.onNotFound(on_NotFound);

    // start http server
    httpServer.begin(80);

#if ENABLE_DEBUG
        Serial.println("http server started");
#endif
}

void handle_httpServer()
{
    httpServer.handleClient();
}

void on_root()
{
#if ENABLE_DEBUG
        Serial.println("call cb_root");
#endif


    String page = FPSTR(html_HEADER);
    page += FPSTR(html_BODY_PRE);

    String body = FPSTR(html_BODY);

    snprintf(convert_buffer, sizeof(convert_buffer), "%i.%i.%i-%i"
        , firmware_version.major
        , firmware_version.major
        , firmware_version.revision
        , firmware_version.bugfix);
    body.replace("{{firmware}}", convert_buffer);

    body.replace("{{wifi_hostname}}", moduleSettings.wifi_hostname);
    body.replace("{{wifi_ssid}}", moduleSettings.wifi_ssid);
    body.replace("{{wifi_password}}", moduleSettings.wifi_password);
    body.replace("{{wifi_use_sta}}", (moduleSettings.wifi_use_sta ? "true" : "false"));

    String serialPort_baud_options;
    for(int idx = 0; idx < baud_range_size; ++idx)
    {
        char const* opTemplate = (
            (moduleSettings.serialPort_baud == baud_range[idx])
             ? baud_range_selected_template
             : baud_range_template);

        sprintf(baud_range_buff, opTemplate, baud_range[idx], baud_range[idx]);
        serialPort_baud_options += baud_range_buff;
    }
    body.replace("{{serialPort_baud_options}}", serialPort_baud_options);

    body.replace("{{serialPort_rxBuffSize}}", String(moduleSettings.serialPort_rxBuffSize).c_str());

    body.replace("{{tcpServer_port}}", String(moduleSettings.tcpServer_port).c_str());
    body.replace("{{tcpServer_buffSize}}", String(moduleSettings.tcpServer_buffSize).c_str());
    body.replace("{{tcpServer_buffSize}}", String(moduleSettings.tcpServer_buffSize).c_str());

    snprintf(convert_buffer, sizeof(convert_buffer), "%i.%i.%i.%i"
        , moduleSettings.static_IPaddress[0]
        , moduleSettings.static_IPaddress[1]
        , moduleSettings.static_IPaddress[2]
        , moduleSettings.static_IPaddress[3]);
    body.replace("{{static_IPaddress}}", convert_buffer);

    snprintf(convert_buffer, sizeof(convert_buffer), "%i.%i.%i.%i"
        , moduleSettings.static_netmask[0]
        , moduleSettings.static_netmask[1]
        , moduleSettings.static_netmask[2]
        , moduleSettings.static_netmask[3]);
    body.replace("{{static_netmask}}", convert_buffer);

    snprintf(convert_buffer, sizeof(convert_buffer), "%i.%i.%i.%i"
        , moduleSettings.static_gateway[0]
        , moduleSettings.static_gateway[1]
        , moduleSettings.static_gateway[2]
        , moduleSettings.static_gateway[3]);
    body.replace("{{static_gateway}}", convert_buffer);

    page += body;
    page += FPSTR(html_BODY_POST);

    httpServer.sendHeader("Content-Length", String(page.length()));
    httpServer.send(200, "text/html", page);
}

void on_save()
{
    if (httpServer.method() != HTTP_POST) {
        httpServer.send(405, "text/plain", "Method Not Allowed");
        return;
    }

#if ENABLE_DEBUG
    Serial.println("POST form was:");
    Serial.printf("   - \"wifi_hostname\"        : \"%s\"\n", httpServer.arg("wifi_hostname").c_str());
    Serial.printf("   - \"wifi_ssid\"            : \"%s\"\n", httpServer.arg("wifi_ssid").c_str());
    Serial.printf("   - \"wifi_password\"        : \"%s\"\n", httpServer.arg("wifi_password").c_str());
    Serial.printf("   - \"wifi_use_sta\"         : \"%s\"\n", httpServer.arg("wifi_use_sta").c_str());
    Serial.printf("   - \"serialPort_baud\"      : \"%s\"\n", httpServer.arg("serialPort_baud").c_str());
    Serial.printf("   - \"serialPort_rxBuffSize\": \"%s\"\n", httpServer.arg("serialPort_rxBuffSize").c_str());
    Serial.printf("   - \"tcpServer_port\"       : \"%s\"\n", httpServer.arg("tcpServer_port").c_str());
    Serial.printf("   - \"tcpServer_buffSize\"   : \"%s\"\n", httpServer.arg("tcpServer_buffSize").c_str());
    Serial.printf("   - \"use_static_ip\"        : \"%s\"\n", httpServer.arg("use_static_ip").c_str());
    Serial.printf("   - \"static_IPaddress\"     : \"%s\"\n", httpServer.arg("static_IPaddress").c_str());
    Serial.printf("   - \"static_netmask\"       : \"%s\"\n", httpServer.arg("static_netmask").c_str());
    Serial.printf("   - \"static_gateway\"       : \"%s\"\n", httpServer.arg("static_gateway").c_str());
#endif

    if(httpServer.hasArg("wifi_hostname"))
    {
        strcpy(moduleSettings.wifi_hostname, httpServer.arg("wifi_hostname").c_str());
    }

    if(httpServer.hasArg("wifi_ssid"))
    {
        strcpy(moduleSettings.wifi_ssid, httpServer.arg("wifi_ssid").c_str());
    }

    if(httpServer.hasArg("wifi_password"))
    {
        strcpy(moduleSettings.wifi_password, httpServer.arg("wifi_password").c_str());
    }

    if(httpServer.hasArg("wifi_use_sta"))
    {
        moduleSettings.wifi_use_sta = (httpServer.arg("wifi_use_sta") == "true");
    }

    if(httpServer.hasArg("serialPort_baud"))
    {
        moduleSettings.serialPort_baud = static_cast<uint32_t>(httpServer.arg("serialPort_baud").toInt());
    }

    if(httpServer.hasArg("serialPort_rxBuffSize"))
    {
        moduleSettings.serialPort_rxBuffSize = static_cast<uint16_t>(httpServer.arg("serialPort_rxBuffSize").toInt());
    }

    if(httpServer.hasArg("tcpServer_port"))
    {
        moduleSettings.tcpServer_port = static_cast<uint16_t>(httpServer.arg("tcpServer_port").toInt());
    }

    if(httpServer.hasArg("tcpServer_buffSize"))
    {
        moduleSettings.tcpServer_buffSize = static_cast<uint16_t>(httpServer.arg("tcpServer_buffSize").toInt());
    }

    if(httpServer.hasArg("use_static_ip"))
    {
        moduleSettings.use_static_ip = (httpServer.arg("use_static_ip") == "true");
    }

    if(httpServer.hasArg("static_IPaddress"))
    {
        toIPAddress(httpServer.arg("static_IPaddress").c_str(), moduleSettings.static_IPaddress);
    }

    if(httpServer.hasArg("static_netmask"))
    {
        toIPAddress(httpServer.arg("static_netmask").c_str(), moduleSettings.static_netmask);
    }

    if(httpServer.hasArg("static_gateway"))
    {
        toIPAddress(httpServer.arg("static_gateway").c_str(), moduleSettings.static_gateway);
    }

    write_settings(moduleSettings);
    httpServer.send(200, "text/plain", String());
}

void on_reboot()
{
#if ENABLE_DEBUG
    Serial.println("Restarting ...");
#endif
    httpServer.send(200, "text/plain", String());
    ESP.restart();
}

void on_update()
{
    // handler for the file upload, gets the sketch bytes, and writes
    // them through the Update object
    HTTPUpload& upload = httpServer.upload();

    if(upload.status == UPLOAD_FILE_START)
    {
        updaterErrorCode = 0;
        updaterErrorDesc.clear();

#if ENABLE_DEBUG
    Serial.println("on_update(); started");
#endif

        // start update with max available size
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        bool isStared = Update.begin(maxSketchSpace, U_FLASH);
        if (!isStared)
        {
            // @todo продумать получение ошибок
            updaterErrorCode = Update.getError();
            updaterErrorDesc = Update.getErrorString();
        }
    }
    else if(upload.status == UPLOAD_FILE_WRITE && updaterErrorCode == 0)
    {
#if ENABLE_DEBUG
        Serial.print('.');
#endif
        size_t writedBytes = Update.write(upload.buf, upload.currentSize);
        if(writedBytes != upload.currentSize)
        {
            // @todo продумать получение ошибок
            updaterErrorCode = Update.getError();
            updaterErrorDesc = Update.getErrorString();
        }
    }
    else if(upload.status == UPLOAD_FILE_END && updaterErrorCode == 0)
    {
        bool isFinish = Update.end(true);
        if(!isFinish)
        {
            // @todo продумать получение ошибок
            updaterErrorCode = Update.getError();
            updaterErrorDesc = Update.getErrorString();
        }
#if ENABLE_DEBUG
        else
        {
            Serial.println();
            Serial.printf("Update Success: %zu\n", upload.totalSize);
        }
#endif
    }
    else if(upload.status == UPLOAD_FILE_ABORTED){
        Update.end();
#if ENABLE_DEBUG
        Serial.println("Update was aborted");
#endif
    }
    esp_yield();
}

void on_update_finish()
{
#if ENABLE_DEBUG
    Serial.println("on_update_finish(); Rebooting...\n");
#endif
    httpServer.sendHeader("Access-Control-Allow-Headers", "*");
    httpServer.sendHeader("Access-Control-Allow-Origin", "*");

    if (Update.hasError())
    {
        httpServer.send(200, F("text/html"), String(F("Update error: ")) + updaterErrorDesc);
        return;
    }

    httpServer.client().setNoDelay(true);

    // @todo имеет смысл отсылать что-то более разумое
    // static const char successResponse[] PROGMEM =  "<META http-equiv=\"refresh\" content=\"15;URL=/\">Update Success! Rebooting...";
    httpServer.send(200, "text/plain", String());

    delay(100);
    httpServer.client().stop();
    ESP.restart();
}


void toIPAddress(const char* str, uint8_t* ipAddress)
{
    int ip_part1; 
    int ip_part2;
    int ip_part3;
    int ip_part4;
    sscanf(str, "%i.%i.%i.%i", &ip_part1, &ip_part2, &ip_part3, &ip_part4);

    ipAddress[0] = ip_part1;
    ipAddress[1] = ip_part2;
    ipAddress[2] = ip_part3;
    ipAddress[3] = ip_part4;
}
/*
void on_NotFound()
{
    digitalWrite(LED_BUILTIN, 1);
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += httpServer.uri();
    message += "\nMethod: ";
    message += (httpServer.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += httpServer.args();
    message += "\n";
    for (uint8_t i = 0; i < httpServer.args(); i++) {
        message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\n";
    }
    httpServer.send(404, "text/plain", message);
    digitalWrite(LED_BUILTIN, 0);
}
*/
