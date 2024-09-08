# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(IDF_PATH $ENV{HOME}/esp/esp-idf)
#set(IDF_PATH $ENV{HOME}/esp/ESP8266_RTOS_SDK)
include(${IDF_PATH}/tools/cmake/project.cmake)
project(wireless-klipper)
