# wireless-klipper
Firmware for MKS Robin WiFi (esp8266) - organizing a `klipper <-> wifi <-> mcu` bridge.

## Source
The firmware source code for esp8266 ([Arduino IDE](https://www.arduino.cc/en/software)) is located in the [wireless_klipper_sta](wireless_klipper_sta) directory.
To build the firmware in the [Arduino IDE](https://www.arduino.cc/en/software), you need to install the libraries for [esp8266](https://github.com/esp8266/Arduino).

In order for the MCU firmware (MKS Robin Nano 1.2) to enable the WiFi module, it is necessary to apply the changes to the MCU Klipper source code - [data/klipper-wifi-enable.patch](data/klipper-wifi-enable.patch).

## Work & Testing
On the server with Klipper, to ensure interaction with the MCU, you need to run [socat](https://linux.die.net/man/1/socat):
```
$ socat -dd pty,link=/tmp/virtualcom,ispeed=b250000,ospeed=b250000,raw,echo=0 TCP4-CONNECT:192.168.1.2:8888,nodelay,forever,interval=1
```

## See also
 - example of automation - [data/systemd](data/systemd), pay attention to [socat.env](data/systemd/socat.env) and [socat@.service](data/systemd/socat@.service]);
 - [ESP support (connection over wifi)](https://klipper.discourse.group/t/esp-support-connection-over-wifi/97)
