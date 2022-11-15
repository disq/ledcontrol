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

# Add your source files
add_executable(${NAME}
    main.cpp ledcontrol.cpp ledcontrol.h util.h config.h encoder.cpp encoder.h
)

# Include required libraries
# This assumes `pimoroni-pico` is stored alongside your project
include(../pimoroni-pico/drivers/button/button)
include(PicoLED/PicoLed.cmake)

target_link_libraries(${NAME}
        pico_stdlib
        button
        hardware_pio
        PicoLed
        hardware_flash
        hardware_sync
        hardware_gpio
        hardware_pwm
        pico_bootsel_via_double_reset
)

if (PICO_CYW43_SUPPORTED) # set by BOARD=pico-w
    if (TARGET pico_cyw43_arch)
        target_link_libraries(${NAME}
                pico_cyw43_arch_none # we need Wifi to access the GPIO, but we don't need anything else
        )
    endif()
endif()

pico_enable_stdio_usb(${NAME} 1)
pico_enable_stdio_uart(${NAME} 0)

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
