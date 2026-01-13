cmake_minimum_required(VERSION 3.13)

# # UART CONFIG - SET PINS + BAUD HERE when using pico_enable_stdio_uart(qrcode 1) below.
## with set(), the definitions have no effect. With target_compile_definitions(), they result in -DPICO_DEFAULT_UART=1, ...
# set(PICO_DEFAULT_UART 1)
# set(PICO_DEFAULT_UART_TX_PIN, 4)     # GP4, pin 6
# set(PICO_DEFAULT_UART_RX_PIN, 5)     # GP5, pin 7
# set(PICO_DEFAULT_UART_BAUD_RATE, 115200)


get_filename_component(PICO_SDK_PATH "../pico-sdk" ABSOLUTE)
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)
project(uart_test C CXX ASM)
pico_sdk_init()

add_executable(uart_test main.c)
target_include_directories(uart_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})	# find tusb_config.h

# add_compile_definitions( 
target_compile_definitions(uart_test PRIVATE
	PICO_DEFAULT_UART=1
	PICO_DEFAULT_UART_TX_PIN=4
	PICO_DEFAULT_UART_RX_PIN=5
)
	#PICO_DEFAULT_UART_BAUD_RATE=115200	# default: 115200

target_link_libraries(uart_test PUBLIC pico_stdlib tinyusb_host)
# hardware_uart)	

pico_add_extra_outputs(uart_test)

pico_enable_stdio_uart(uart_test 1)     # 0: Disable, 1: Enable UART TX RX pins		#define PICO_STDIO_UART_ENABLED
pico_enable_stdio_usb(uart_test 0)      # 0: Disable, 1: Enable USB serial		#define PICO_STDIO_USB_USE_TINYUSB
