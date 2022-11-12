cmake_minimum_required(VERSION 3.12)

# Change your executable name to something creative!
set(NAME ledcontrol) # <-- Name your project/executable here!

include(pimoroni_pico_import.cmake)
include(pico_sdk_import.cmake)

# Gooey boilerplate
project(${NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_CXX_ENABLE_EXCEPTIONS 1)
set(PICO_CXX_ENABLE_RTTI 1)

# Initialize the SDK
pico_sdk_init()

# Add your source files
add_executable(${NAME}
    main.cpp ledcontrol.cpp ledcontrol.h util.h config.h
)

# Include required libraries
# This assumes `pimoroni-pico` is stored alongside your project
include(../pimoroni-pico/common/pimoroni_i2c)
include(../pimoroni-pico/drivers/ioexpander/ioexpander)
include(../pimoroni-pico/libraries/breakout_encoder/breakout_encoder)
include(../pimoroni-pico/drivers/button/button)
include(PicoLED/PicoLed.cmake)

# Don't forget to link the libraries you need!
target_link_libraries(${NAME}
        pico_stdlib
        pimoroni_i2c
        breakout_encoder
        button
        hardware_pio
        PicoLed
        pico_bootsel_via_double_reset
)

if (PICO_CYW43_SUPPORTED)
    if (TARGET pico_cyw43_arch)
        target_link_libraries(${NAME}
                pico_cyw43_arch_none # we need Wifi to access the GPIO, but we don't need anything else
        )
    endif()
endif()

# create map/bin/hex file etc.
pico_add_extra_outputs(${NAME})
pico_enable_stdio_usb(${NAME} ENABLED)

# Set up files for the release packages
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.uf2
    ${CMAKE_CURRENT_LIST_DIR}/README.md
    DESTINATION .
)

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_GENERATOR "ZIP" "TGZ")
include(CPack)
