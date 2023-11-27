cmake_minimum_required(VERSION 3.12)

set(PICO_INSTALL_PATH "C:\\Program Files\\Raspberry Pi\\Pico SDK v1.5.1") # Change this to match your install path

set(PICO_SDK_PATH ${PICO_INSTALL_PATH}/pico-sdk)
set(CMAKE_TOOLCHAIN_FILE ${PICO_INSTALL_PATH}/pico-sdk/cmake/preload/toolchains/pico_arm_gcc.cmake)
set(PICO_TOOLCHAIN_PATH ${PICO_INSTALL_PATH}/gcc-arm-none-eabi/bin)
set(CMAKE_MAKE_PROGRAM ${PICO_INSTALL_PATH}/ninja/ninja.exe)

set(PICO_BOARD pico_w)
set(PICO_CYW43_SUPPORTED 1)

include(pico_sdk_import.cmake)

set(PROJECT pico_car)

project(${PROJECT} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

pico_sdk_init()

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        )
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-maybe-uninitialized)
endif()

message("PICO_LWIP_CONTRIB_PATH: ${PICO_LWIP_CONTRIB_PATH}")
message("CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")

if (DEFINED ENV{WIFI_SSID} AND (NOT WIFI_SSID))
    set(WIFI_SSID $ENV{WIFI_SSID})
    message("Using WIFI_SSID from environment ('${WIFI_SSID}')")
endif()

if (DEFINED ENV{WIFI_PASSWORD} AND (NOT WIFI_PASSWORD))
    set(WIFI_PASSWORD $ENV{WIFI_PASSWORD})
    message("Using WIFI_PASSWORD from environment")
endif()

set(WIFI_SSID "${WIFI_SSID}" CACHE INTERNAL "WiFi SSID for examples")
set(WIFI_PASSWORD "${WIFI_PASSWORD}" CACHE INTERNAL "WiFi password for examples")

# FreeRTOS Include
if (NOT FREERTOS_KERNEL_PATH AND NOT DEFINED ENV{FREERTOS_KERNEL_PATH})
    message("Skipping Pico W FreeRTOS examples as FREERTOS_KERNEL_PATH not defined")
else()
    include(FreeRTOS_Kernel_import.cmake)
endif()



# No sys version
add_executable(${PROJECT}_nosys 
    car_wifi.c
    )
target_compile_definitions(${PROJECT}_nosys PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    )
target_include_directories(${PROJECT}_nosys PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    )
target_link_libraries(${PROJECT}_nosys
    pico_cyw43_arch_lwip_threadsafe_background
    pico_stdlib
    pico_lwip_iperf
    FreeRTOS-Kernel-Heap4
    hardware_pwm
    )
pico_enable_stdio_usb(${PROJECT}_nosys 1)
pico_add_extra_outputs(${PROJECT}_nosys)

# # Sys version
# add_executable(${PROJECT}_sys 
#     car_wifi.c
#     )
# target_compile_definitions(${PROJECT}_sys PRIVATE
#     WIFI_SSID=\"${WIFI_SSID}\"
#     WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
#     NO_SYS=1
#     LWIP_SOCKET=1
#     PING_USE_SOCKETS=1
#     )
# target_include_directories(${PROJECT}_sys PRIVATE
#     ${CMAKE_CURRENT_LIST_DIR}
#     ${PICO_LWIP_CONTRIB_PATH}/apps/ping
#     )
# target_link_libraries(${PROJECT}_sys
#     pico_cyw43_arch_lwip_sys_freertos
#     pico_stdlib
#     pico_lwip_iperf
#     FreeRTOS-Kernel-Heap4
#     )
# pico_enable_stdio_usb(${PROJECT}_sys 1)
# pico_add_extra_outputs(${PROJECT}_sys)