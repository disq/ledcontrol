cmake_minimum_required(VERSION 3.12)

# Change your executable name to something creative!
set(NAME ledcontrol) # <-- Name your project/executable here!

include(pimoroni_pico_import.cmake)
include(pico_sdk_import.cmake)

# Gooey boilerplate
project(${NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

set(PICO_CXX_ENABLE_EXCEPTIONS 1)
set(PICO_CXX_ENABLE_RTTI 1)

# Initialize the SDK
pico_sdk_init()

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function
        -Wno-maybe-uninitialized
        )

if ((PICO_CYW43_SUPPORTED) AND (TARGET pico_cyw43_arch))
    add_executable(${NAME}
            main.cpp ledcontrol.cpp ledcontrol.h util.h config.h encoder.cpp encoder.h iot.cpp iot.h presence.cpp presence.h config_iot.h mbedtls_util.c cJSON/cJSON.c cJSON/cJSON.h DFRobot_mmWave_Radar.cpp DFRobot_mmWave_Radar.h
        )
else()
    add_executable(${NAME}
            main.cpp ledcontrol.cpp ledcontrol.h util.h config.h encoder.cpp encoder.h presence.cpp presence.h DFRobot_mmWave_Radar.cpp DFRobot_mmWave_Radar.h
        )
endif()

# Include required libraries
# This assumes `pimoroni-pico` is stored alongside your project
include(../pimoroni-pico/drivers/button/button)
include(../pimoroni-pico/drivers/plasma/plasma)

target_link_libraries(${NAME}
        pico_stdlib
        button
        plasma
        hardware_flash
        hardware_sync
        hardware_gpio
        hardware_pwm
        hardware_uart
        pico_bootsel_via_double_reset
)

if ((PICO_CYW43_SUPPORTED) AND (TARGET pico_cyw43_arch)) # set by BOARD=pico-w
    set(WIFI_SSID "${WIFI_SSID}" CACHE INTERNAL "WiFi SSID")
    set(WIFI_PASSWORD "${WIFI_PASSWORD}" CACHE INTERNAL "WiFi password")

    if ("${WIFI_SSID}" STREQUAL "")
        message(FATAL_ERROR "Skipping build as WIFI_SSID is not defined")
    elseif ("${WIFI_PASSWORD}" STREQUAL "")
        message(FATAL_ERROR "Skipping build as WIFI_PASSWORD is not defined")
    endif()

    target_compile_definitions(${NAME} PRIVATE
            WIFI_SSID=\"${WIFI_SSID}\"
            WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        )

    target_include_directories(${NAME} PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}
            ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
        )

    target_link_libraries(${NAME}
            pico_cyw43_arch_lwip_poll
#            pico_cyw43_arch_lwip_threadsafe_background
            pico_lwip_mbedtls
            pico_mbedtls
            pico_lwip_mqtt
            pico_unique_id
        )
endif()

pico_enable_stdio_usb(${NAME} 1)
pico_enable_stdio_uart(${NAME} 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(${NAME})

# Set up files for the release packages
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.uf2
        ${CMAKE_CURRENT_LIST_DIR}/README.md
        DESTINATION .
        )

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_GENERATOR "ZIP" "TGZ")
include(CPack)
