#!/bin/bash

idf_target="esp32c2"
idf_home="${HOME}/esp/esp-idf-v5.3.2"
build_dir="cmake-build.${idf_target}-release"

cp sdkconfigs/sdkconfig.${idf_target}.release.full sdkconfig

[ -d ${build_dir} ] && rm -rf ${build_dir}
mkdir -p ${build_dir}

pushd ${build_dir}
source ${idf_home}/export.sh
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build $PWD
popd
