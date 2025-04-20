#!/bin/bash

idf_target="esp8266"
idf_home="${HOME}/esp/ESP8266_RTOS_SDK"
build_dir="cmake-build.${idf_target}-release"

cp sdkconfigs/sdkconfig.${idf_target}.release.full sdkconfig

[ -d ${build_dir} ] && rm -rf ${build_dir}
mkdir -p ${build_dir}

pushd ${build_dir}
source ${idf_home}/export.sh
cmake ..
cmake --build $PWD
popd
