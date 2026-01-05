cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
project(blink C CXX ASM)

pico_sdk_init()

add_executable(blink
    main.c
)

pico_enable_stdio_usb(blink 1)      # Enable USB serial
pico_enable_stdio_uart(blink 0)     # Disable UART TX RX pins
target_link_libraries(blink pico_stdlib)

pico_add_extra_outputs(blink)
set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS, 1000)  # Then stdio_init_all() waits up to 1s


