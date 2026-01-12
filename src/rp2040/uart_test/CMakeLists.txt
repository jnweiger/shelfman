cmake_minimum_required(VERSION 3.13)
get_filename_component(PICO_SDK_PATH "../pico-sdk" ABSOLUTE)
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)
project(uart_test C CXX ASM)
pico_sdk_init()

add_executable(uart_test main.c)
target_link_libraries(uart_test PUBLIC pico_stdlib)  # NO tinyusb_host

pico_add_extra_outputs(uart_test)

