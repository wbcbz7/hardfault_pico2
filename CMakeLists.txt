# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")
#set(PICO_COPY_TO_RAM 1)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(dh2025w_demo C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()


add_executable(dh2025w_demo
        main.cpp
        dvi.c
        lxmplay.cpp
	core1.cpp
        )

pico_generate_pio_header(dh2025w_demo ${CMAKE_CURRENT_LIST_DIR}/dvi.pio)
pico_generate_pio_header(dh2025w_demo ${CMAKE_CURRENT_LIST_DIR}/i2s.pio)

target_include_directories(dh2025w_demo PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/images
        )

pico_enable_stdio_uart(dh2025w_demo 1)
pico_enable_stdio_usb(dh2025w_demo 0)

# pull in common dependencies
target_link_libraries(dh2025w_demo
        pico_stdlib
        pico_multicore
        hardware_pio
        hardware_dma
        hardware_pll
        hardware_vreg
        hardware_clocks
        hardware_gpio
        pico_sync
        pico_util
        )

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(dh2025w_demo)
