# assuming PICO_BOARD=pico.
# specify -DPICO_BOARD=pico_w                  # for a Pico W (RP2040 + CYW43439 WiFi)
# or      -DPICO_BOARD=adafruit_feather_rgb    # ...

cmake_minimum_required(VERSION 3.13)

get_filename_component(PICO_SDK_PATH "../pico-sdk" ABSOLUTE)
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)
project(blink C CXX ASM)

pico_sdk_init()

add_executable(blink_pico main.c)
target_compile_definitions(blink_pico PRIVATE TARGET_PICO=1)

target_link_libraries(blink_pico pico_stdlib)
pico_enable_stdio_usb(blink_pico 1)      # Enable USB serial
pico_enable_stdio_uart(blink_pico 0)     # Disable UART TX RX pins

pico_add_extra_outputs(blink_pico)

