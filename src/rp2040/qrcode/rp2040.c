#include "rp2040.h"

// compatibility layer
int32_t getrandom(uint32_t *r, size_t n, int _unused)
{
	assert(n == 4);
    *r = get_rand_32();
    return n;
}

// From pico-examples/common/get_bootsel_button.c
bool __no_inline_not_in_flash_func(get_bootsel_button)() {
    const uint CS_PIN_INDEX = 1;

    // Disable interrupts (flash access might be interrupted)
    uint32_t flags = save_and_disable_interrupts();

    // Float the flash CS pin (Hi-Z)
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
        GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
        IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    // Small delay (can't sleep, no flash access)
    for (volatile int i = 0; i < 1000; ++i);

    // Read pin state via SIO (button pulls low when pressed)
    bool button_state = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));

    // Restore flash CS
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
        GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
        IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    restore_interrupts(flags);
    return button_state;
}
