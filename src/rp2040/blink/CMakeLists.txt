cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
project(blink C CXX ASM)

pico_sdk_init()

add_executable(blink
    main.c
)

target_link_libraries(blink pico_stdlib)

pico_add_extra_outputs(blink)

