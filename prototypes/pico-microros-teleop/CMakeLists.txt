cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico_w CACHE STRING "Target board")

if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()

if(NOT PICO_SDK_PATH)
    message(FATAL_ERROR "Define PICO_SDK_PATH")
endif()

include("${PICO_SDK_PATH}/pico_sdk_init.cmake")

project(remote_cart_teleop C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(
    MICROROS_LIB_DIR
    "$ENV{MICROROS_LIB_DIR}"
    CACHE PATH
    "Directory containing libmicroros.a and include/"
)

set(
    REMOTE_CART_WIFI_SSID
    "YOUR_WIFI_SSID"
    CACHE STRING
    "Wi-Fi network name"
)

set(
    REMOTE_CART_WIFI_PASSWORD
    "YOUR_WIFI_PASSWORD"
    CACHE STRING
    "Wi-Fi network password"
)

set(
    REMOTE_CART_AGENT_IP
    "10.42.0.1"
    CACHE STRING
    "micro-ROS Agent IPv4 address"
)

set(
    REMOTE_CART_AGENT_PORT
    "8888"
    CACHE STRING
    "micro-ROS Agent UDP port"
)

if(NOT EXISTS "${MICROROS_LIB_DIR}/libmicroros.a")
    message(FATAL_ERROR "libmicroros.a not found in MICROROS_LIB_DIR")
endif()

if(NOT EXISTS "${MICROROS_LIB_DIR}/include")
    message(FATAL_ERROR "micro-ROS headers not found")
endif()

configure_file(
    "config/remote_cart_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/remote_cart_config.h"
    @ONLY
)

add_executable(
    pico_car_teleop
    "src/main.c"
    "src/microros_app.c"
    "src/motor_control.c"
    "src/picow_udp_transport.c"
)

target_include_directories(
    pico_car_teleop
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/inc"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
        "${MICROROS_LIB_DIR}/include"
)

target_link_libraries(
    pico_car_teleop
    PRIVATE
        pico_stdlib
        pico_cyw43_arch_lwip_threadsafe_background
        hardware_clocks
        hardware_gpio
        hardware_pwm
        "${MICROROS_LIB_DIR}/libmicroros.a"
        m
)

target_compile_options(
    pico_car_teleop
    PRIVATE
        -Wall
        -Wextra
)

pico_enable_stdio_usb(pico_car_teleop 1)
pico_enable_stdio_uart(pico_car_teleop 0)
pico_add_extra_outputs(pico_car_teleop)
