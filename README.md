The documentation needs to be updated :'(


# wireless-klipper

Firmware for esp8684 module - organizing a  `klipper <-> wifi <-> mcu`  bridge.
The esp8684 module was chosen to replace the esp8266 module (ESP 12F). The whole problem is that the latter does not keep the communication channel stable enough
How to replace:
- take the MKS Robin Wifi module
- solder the esp8266 board out of it
- solder the esp8684 module instead

## Source

The firmware source code for esp8684 is located in the  [main](/main)  directory.
To build the firmware, you download and need to install the libraries for [esp_idf](https://github.com/espressif/esp-idf).

In order for the MCU firmware (MKS Robin Nano 1.2) to enable the WiFi module,
it is necessary to apply the changes to the MCU Klipper source code - [klipper-wifi-enable_0.1.0.patch](/data/klipper-wifi-enable_0.1.0.patch).

## How flash MKS Robin Wi-Fi

[](https://github.com/apollo80/wireless-klipper/tree/develop#how-flash-mks-robin-wi-fi)

The MKS Robin Wi-Fi is essentially an ESP8266 performed by ESP12F. There are quite a lot of options and examples of firmware for ESP12F on the Internet. It is flashed via USB ->RX/TX converter with 3.3V output power.

As one of the examples of ESP12F firmware, I can give the following links:  [https://forum.micropython.org/viewtopic.php?t=7057](https://forum.micropython.org/viewtopic.php?t=7057)  [https://www.youtube.com/watch?v=R-FDJsuH_eE](https://www.youtube.com/watch?v=R-FDJsuH_eE)

## How to patch a klipper

[](https://github.com/apollo80/wireless-klipper/tree/develop#how-to-patch-a-klipper)

It's simple enough. Just copy the  [data/klipper-wifi-enable_0.1.0.patch](/data/klipper-wifi-enable_0.1.0.patch)  file to the root of klipper's sources and run the command:

patch -p1 -i klipper-wifi-enable_0.1.0.patch

In general, it will look like this:

<login>@<hostname>:~/$ git clone https://github.com/Klipper3d/klipper
...
<login>@<hostname>:~/tmp$ git clone https://github.com/apollo80/wireless-klipper
...
<login>@<hostname>:~/tmp$ cp wireless-klipper/data/klipper-wifi-enable_0.1.0.patch ~/klipper/
<login>@<hostname>:~/tmp$ cd klipper/
<login>@<hostname>:~/klipper$ patch -p1 -i klipper-wifi-enable_0.1.0.patch
patching file src/stm32/Kconfig
patching file src/stm32/Makefile
patching file src/stm32/serial.c
<login>@<hostname>:~/klipper$

Next, you need to configure the firmware by enabling the Wi-Fi option. By default, this functionality is disabled.  [![klipper-config__enable_wifi](https://github.com/apollo80/wireless-klipper/raw/develop/data/klipper-config__enable_wifi.png)](https://github.com/apollo80/wireless-klipper/blob/develop/data/klipper-config__enable_wifi.png)

## Configure klipper

[](https://github.com/apollo80/wireless-klipper/tree/develop#configure-klipper)

To interact with the mcu over the network, you need to specify the connection parameters to the mcu in the klipper configuration file:

```
[mcu]
#host: 192.168.4.44
host: printer_ghost5.local
port: 8888
restart_method: command

```

## See also

[](https://github.com/apollo80/wireless-klipper/tree/develop#see-also)

-   example of automation -  [data/systemd](https://github.com/apollo80/wireless-klipper/blob/develop/data/systemd);
-   [ESP support (connection over wifi)](https://klipper.discourse.group/t/esp-support-connection-over-wifi/97)
