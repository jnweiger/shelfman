# assuming PICO_BOARD=pico.
# specify -DPICO_BOARD=pico_w                  # for a Pico W (RP2040 + CYW43439 WiFi)
# or      -DPICO_BOARD=adafruit_feather_rgb    # ...

cmake_minimum_required(VERSION 3.13)

set(QRCODE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../QR-Code-generator/c)
set(PTOUCH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../ptouch-print/src)
set(GFXFONT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../Adafruit-GFX-Library)

# # UART CONFIG - SET PINS + BAUD HERE when using pico_enable_stdio_uart(qrcode 1) below.
## with set(), the definitions have no effect. Wht add_compile_definitions(), they result in -DPICO_DEFAULT_UART=1, ...
# set(PICO_DEFAULT_UART 1)
# set(PICO_DEFAULT_UART_TX_PIN, 4)     # GP4, pin 6
# set(PICO_DEFAULT_UART_RX_PIN, 5)     # GP5, pin 7
# set(PICO_DEFAULT_UART_BAUD_RATE, 115200)

get_filename_component(PICO_SDK_PATH "../pico-sdk" ABSOLUTE)
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)
project(qrcode C CXX ASM)

pico_sdk_init()

add_executable(qrcode
	../../shelfman-qrcode.c
	rp2040.c
	${QRCODE_DIR}/qrcodegen.c
)

target_compile_definitions(qrcode PRIVATE
	TARGET_PICO=1
	WITH_PNG_SUPPORT=0
	PICO_DEFAULT_UART=1
	PICO_DEFAULT_UART_TX_PIN=4
	PICO_DEFAULT_UART_RX_PIN=5
)
	#PICO_DEFAULT_UART_BAUD_RATE=115200	# default: 115200

target_include_directories(qrcode PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR} 				# find tusb_config.h
	${QRCODE_DIR} ${PTOUCH_DIR} ${GFXFONT_DIR}
)
#	${PICO_SDK_PATH}/src/rp2_common/hardware_rng/include
#    	${PICO_SDK_PATH}/src/common/pico_base/include

target_link_libraries(qrcode
	pico_stdlib
	pico_rand
)
#    	tinyusb_host

pico_enable_stdio_usb(qrcode 0)      # 0: Disable, 1: Enable USB serial		#if PICO_STDIO_USB_USE_TINYUSB
pico_enable_stdio_uart(qrcode 1)     # 0: Disable, 1: Enable UART TX RX pins	#if PICO_STDIO_UART_ENABLED

pico_add_extra_outputs(qrcode)

