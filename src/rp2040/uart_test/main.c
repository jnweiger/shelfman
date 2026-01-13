/*
 * STDIO requires:
 *
   add_compile_definitions( PICO_DEFAULT_UART=1 )
   target_link_libraries(uart_test PUBLIC pico_stdlib)	# hardware_uart or tinyusb_host not needed, but harmless here.
   pico_enable_stdio_usb(uart_test 0)
   pico_enable_stdio_uart(uart_test 1)
 */

#define RAW_UART 0		// 1: test raw uart, 0: test stdio via uart.

#include "pico/stdlib.h"	// sleep_ms(), stdio_init_all()
#if RAW_UART
#include "hardware/uart.h"
#include "hardware/gpio.h"
#else
# include <stdio.h>		// for printf()
#endif

int main(void)
{
#if RAW_UART
    // 1. GPIO FIRST
    gpio_init(4);
    gpio_init(5);
    gpio_set_function(4, GPIO_FUNC_UART);  // UART1 TX
    gpio_set_function(5, GPIO_FUNC_UART);  // UART1 RX

    // 2. UART SECOND
    uart_init(uart1, 115200);
    uart_set_hw_flow(uart1, false, false);
    uart_set_format(uart1, 8, 1, UART_PARITY_NONE);
#else
    // uart_init(uart1, 115200);			  // not needed, done by mandatory add_compile_definitions(PICO_DEFAULT_UART=1)
    // gpio_set_function(4, GPIO_FUNC_UART);  // UART1 TX, needed unless add_compile_definitions(PICO_DEFAULT_UART_TX_PIN=4)
    // gpio_set_function(5, GPIO_FUNC_UART);  // UART1 RX, needed unless add_compile_definitions(PICO_DEFAULT_UART_RX_PIN=5)

    // uart_set_hw_flow(uart1, false, false);	// default: disabled RTS/CTS
    // uart_set_format(uart1, 8, 1, UART_PARITY_NONE);	// default: 8N1
	// uart_set_fifo_enabled(uart1, true);		// default: fifo enabled

    stdio_init_all();		// uart nr. and baud rate chosen in CMakeLists.txt via target_compile_definitions()
#endif

    while (1) {
#if RAW_UART
        uart_puts(uart1, "RAW TX TEST\n");
#else
        printf("STDIO TX TEST\n");
#endif
        sleep_ms(1000);
    }
}

