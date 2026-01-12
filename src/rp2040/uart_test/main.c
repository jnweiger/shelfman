#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

int main(void)
{
    // 1. GPIO FIRST
    gpio_init(4);
    gpio_init(5);
    gpio_set_function(4, GPIO_FUNC_UART);  // UART1 TX
    gpio_set_function(5, GPIO_FUNC_UART);  // UART1 RX

    // 2. UART SECOND
    uart_init(uart1, 115200);
    uart_set_hw_flow(uart1, false, false);
    uart_set_format(uart1, 8, 1, UART_PARITY_NONE);

    while (1) {
        uart_puts(uart1, "RAW TX TEST\n");
        sleep_ms(1000);
    }
}

