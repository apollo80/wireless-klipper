#!/bin/bash

cp sdkconfig.release sdkconfig
[ -d cmake-build-release ] && rm -rf cmake-build-release
mkdir -p cmake-build-release

pushd cmake-build-release
source ${HOME}/esp/ESP8266_RTOS_SDK/export.sh
cmake ..
cmake --build $PWD
popd
