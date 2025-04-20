# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

#if (CMAKE_BUILD_TYPE STREQUAL "Release")
#    add_compile_options(-DNDEBUG)
#    add_definitions(-DNDEBUG)
#endif()

#set(IDF_PATH $ENV{HOME}/esp/esp-idf-v5.3.2)
#set(IDF_PATH $ENV{HOME}/esp/ESP8266_RTOS_SDK)
set(IDF_PATH $ENV{IDF_PATH})
include(${IDF_PATH}/tools/cmake/project.cmake)

project(wireless-klipper)
if (${IDF_TARGET} STREQUAL "esp32c2")
    idf_build_set_property(COMPILE_DEFINITIONS "dhcp_network_changed=dhcp_network_changed_link_up" APPEND)
endif()


