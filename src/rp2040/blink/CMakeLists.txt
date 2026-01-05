# assuming PICO_BOARD=pico.
# specify -DPICO_BOARD=pico_w      		# for a Pico W (RP2040 + CYW43439 WiFi)
# or      -DPICO_BOARD=adafruit_feather_rgb 	# ...

cmake_minimum_required(VERSION 3.13)

project(blink C CXX ASM)

# # Linux native target
# add_executable(blink_linux main.c)
# target_compile_definitions(blink_linux PRIVATE TARGET_LINUX=1)

# RP2040 target (if PICO_SDK_PATH set)
# Set SDK path relative to project
get_filename_component(PICO_SDK_PATH "../pico-sdk" ABSOLUTE)
if (EXISTS ${PICO_SDK_PATH})
    include(${PICO_SDK_PATH}/pico_sdk_init.cmake)
    pico_sdk_init()

    add_executable(blink_pico main.c)
    target_compile_definitions(blink_pico PRIVATE TARGET_PICO=1)
    pico_enable_stdio_usb(blink_pico 1)      # Enable USB serial
    pico_enable_stdio_uart(blink_pico 0)     # Disable UART TX RX pins
    target_link_libraries(blink_pico pico_stdlib)
    pico_add_extra_outputs(blink_pico)
    set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS, 1000)  # Then stdio_init_all() waits up to 1s
else()
    message(WARNING "Pico SDK not found at ${PICO_SDK_PATH}")
endif()

