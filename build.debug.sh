#!/bin/bash

cp sdkconfig.release sdkconfig
[ -d cmake-build-debug ] && rm -rf cmake-build-debug
mkdir -p cmake-build-debug

pushd cmake-build-debug
source ${HOME}/esp/ESP8266_RTOS_SDK/export.sh
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build $PWD
popd
